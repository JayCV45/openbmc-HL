#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <chrono>
#include <csignal>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
#include <limits>
#include <syslog.h>

namespace
{

constexpr const char* kTemperaturePath =
    "/sys/class/thermal/thermal_zone0/temp";

constexpr const char* kServiceName =
    "xyz.openbmc_project.CustomTemperature";

constexpr const char* kObjectPath =
    "/xyz/openbmc_project/sensors/temperature/CPU_Temp";

constexpr const char* kSensorInterface =
    "xyz.openbmc_project.Sensor.Value";

constexpr const char* kTemperatureUnit =
    "xyz.openbmc_project.Sensor.Value.Unit.DegreesC";

constexpr const char* kChassisPath =
    "/xyz/openbmc_project/inventory/system/chassis";

constexpr const char* kAssociationInterface =
    "xyz.openbmc_project.Association.Definitions";

// Demonstration temperature thresholds (Celsius).
constexpr double kWarningHigh = 40.0;
constexpr double kCriticalHigh = 75.0;

// Alarm recovery hysteresis (Celsius).
constexpr double kHysteresis = 2.0;

// Standard OpenBMC threshold interfaces.
constexpr const char* kWarningInterface =
    "xyz.openbmc_project.Sensor.Threshold.Warning";

constexpr const char* kCriticalInterface =
    "xyz.openbmc_project.Sensor.Threshold.Critical";

constexpr auto kPollingInterval = std::chrono::seconds(2);

class ThresholdMonitor
{
  public:
    struct State
    {
        bool warning;
        bool critical;
    };

    State update(double temperature)
    {
        // Warning alarm with 2°C hysteresis.
        warningActive = updateAlarm(
            temperature,
            kWarningHigh,
            warningActive
        );

        // Critical alarm with 2°C hysteresis.
        criticalActive = updateAlarm(
            temperature,
            kCriticalHigh,
            criticalActive
        );

        return {warningActive, criticalActive};
    }

  private:
    static bool updateAlarm(
        double temperature,
        double threshold,
        bool active)
    {
        if (!active)
        {
            // Assert when the temperature reaches the limit.
            return temperature >= threshold;
        }

        // Clear only after falling below the recovery limit.
        return temperature > (threshold - kHysteresis);
    }

    bool warningActive = false;
    bool criticalActive = false;
};

// Read Raspberry Pi CPU temperature in Celsius.
double readTemperature()
{
    std::ifstream sensorFile(kTemperaturePath);

    long temperatureMilliCelsius = 0;

    if (!(sensorFile >> temperatureMilliCelsius))
    {
        throw std::runtime_error(
            "Failed to read CPU temperature");
    }

    return static_cast<double>(
        temperatureMilliCelsius) / 1000.0;
}

} // namespace

int main()
{
    try
    {
        // Read a valid initial value before publishing the sensor.
        const double initialTemperature = readTemperature();

        // Event loop for D-Bus and periodic sensor updates.
        boost::asio::io_context io;

        // Connect to the system D-Bus.
        auto connection =
            std::make_shared<sdbusplus::asio::connection>(io);

        // Register our service name on D-Bus.
        connection->request_name(kServiceName);

        // Create the D-Bus object server.
        sdbusplus::asio::object_server objectServer(connection);

        // Create the temperature sensor interface.
        auto sensorInterface = objectServer.add_interface(
            kObjectPath,
            kSensorInterface
        );

        // Publish the initial temperature.
        sensorInterface->register_property(
            "Value",
            initialTemperature
        );

        // Publish the temperature unit.
        sensorInterface->register_property(
            "Unit",
            std::string(kTemperatureUnit)
        );

        // Make the interface available on D-Bus.
        if (!sensorInterface->initialize())
        {
            throw std::runtime_error(
                "Failed to initialize sensor interface");
        }

        const double noThreshold =
            std::numeric_limits<double>::quiet_NaN();

        // Create the warning threshold interface.
        auto warningInterface = objectServer.add_interface(
            kObjectPath,
            kWarningInterface
        );

        warningInterface->register_property(
            "WarningHigh",
            kWarningHigh,
            sdbusplus::asio::PropertyPermission::readOnly
        );

        warningInterface->register_property(
            "WarningLow",
            noThreshold,
            sdbusplus::asio::PropertyPermission::readOnly
        );

        warningInterface->register_property(
            "WarningAlarmHigh",
            false,
            sdbusplus::asio::PropertyPermission::readOnly
        );

        warningInterface->register_property(
            "WarningAlarmLow",
            false,
            sdbusplus::asio::PropertyPermission::readOnly
        );

        if (!warningInterface->initialize())
        {
            throw std::runtime_error(
                "Failed to initialize warning threshold interface");
        }

        // Create the critical threshold interface.
        auto criticalInterface = objectServer.add_interface(
            kObjectPath,
            kCriticalInterface
        );

        criticalInterface->register_property(
            "CriticalHigh",
            kCriticalHigh,
            sdbusplus::asio::PropertyPermission::readOnly
        );

        criticalInterface->register_property(
            "CriticalLow",
            noThreshold,
            sdbusplus::asio::PropertyPermission::readOnly
        );

        criticalInterface->register_property(
            "CriticalAlarmHigh",
            false,
            sdbusplus::asio::PropertyPermission::readOnly
        );

        criticalInterface->register_property(
            "CriticalAlarmLow",
            false,
            sdbusplus::asio::PropertyPermission::readOnly
        );

        if (!criticalInterface->initialize())
        {
            throw std::runtime_error(
                "Failed to initialize critical threshold interface");
        }

        // Link this temperature sensor to the Raspberry Pi chassis.
        using Association =
            std::tuple<std::string, std::string, std::string>;

        const std::vector<Association> associations{
            {
                "chassis",
                "all_sensors",
                kChassisPath
            }
        };

        // Publish association metadata on the sensor object.
        auto associationInterface = objectServer.add_interface(
            kObjectPath,
            kAssociationInterface
        );

        associationInterface->register_property(
            "Associations",
            associations
        );

        if (!associationInterface->initialize())
        {
            throw std::runtime_error(
                "Failed to initialize chassis association");
        }

        // Timer for periodic temperature readings.
        boost::asio::steady_timer timer(io);

        ThresholdMonitor thresholdMonitor;

        // Track the most recently published states.
        bool previousWarning = false;
        bool previousCritical = false;

        // Send transition events to the system journal.
        openlog("temperature-reader", LOG_PID, LOG_DAEMON);

        std::function<void()> updateTemperature;

        updateTemperature = [&]()
        {
            try
            {
                const double temperature = readTemperature();

                // Update D-Bus property.
                sensorInterface->set_property(
                    "Value",
                    temperature
                );

                // Evaluate warning and critical thresholds.
                const auto state = thresholdMonitor.update(temperature);

                // Update D-Bus only when an alarm changes.
                if (state.warning != previousWarning)
                {
                    warningInterface->set_property(
                        "WarningAlarmHigh",
                        state.warning
                    );

                    syslog(
                        state.warning ? LOG_WARNING : LOG_INFO,
                        "CPU temperature WARNING %s: %.2f C (threshold %.2f C)",
                        state.warning ? "asserted" : "cleared",
                        temperature,
                        kWarningHigh
                    );

                    previousWarning = state.warning;
                }

                if (state.critical != previousCritical)
                {
                    criticalInterface->set_property(
                        "CriticalAlarmHigh",
                        state.critical
                    );

                    syslog(
                        state.critical ? LOG_CRIT : LOG_INFO,
                        "CPU temperature CRITICAL %s: %.2f C (threshold %.2f C)",
                        state.critical ? "asserted" : "cleared",
                        temperature,
                        kCriticalHigh
                    );

                    previousCritical = state.critical;
                }

                // Keep journal logging for debugging.
                std::cout << std::fixed
                          << std::setprecision(2)
                          << "CPU Temperature: "
                          << temperature << " C"
                          << std::endl;
            }
            catch (const std::exception& error)
            {
                std::cerr << "Sensor error: "
                          << error.what()
                          << std::endl;
            }

            // Schedule the next temperature reading.
            timer.expires_after(kPollingInterval);

            timer.async_wait(
                [&](const boost::system::error_code& error)
                {
                    if (!error)
                    {
                        updateTemperature();
                    }
                }
            );
        };

        // Handle service shutdown.
        boost::asio::signal_set signals(
            io, SIGINT, SIGTERM
        );

        signals.async_wait(
            [&](const boost::system::error_code&, int)
            {
                timer.cancel();
                io.stop();
            }
        );

        std::cout << "Temperature D-Bus service started"
                  << std::endl;

        updateTemperature();

        // Process D-Bus requests and timer events.
        io.run();

        closelog();
        
        std::cout << "Temperature D-Bus service stopped"
                  << std::endl;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Fatal error: "
                  << error.what()
                  << std::endl;

        return 1;
    }

    return 0;
}
