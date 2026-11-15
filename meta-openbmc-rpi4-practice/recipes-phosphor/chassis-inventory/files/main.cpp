#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <csignal>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{

constexpr const char* kServiceName =
    "xyz.openbmc_project.CustomChassis";

constexpr const char* kInventoryRoot =
    "/xyz/openbmc_project/inventory";

constexpr const char* kChassisPath =
    "/xyz/openbmc_project/inventory/system/chassis";

constexpr const char* kItemInterface =
    "xyz.openbmc_project.Inventory.Item";

constexpr const char* kChassisInterface =
    "xyz.openbmc_project.Inventory.Item.Chassis";

} // namespace

int main()
{
    try
    {
        boost::asio::io_context io;

        // Connect to OpenBMC's system D-Bus.
        auto connection =
            std::make_shared<sdbusplus::asio::connection>(io);

        connection->request_name(kServiceName);

        // We explicitly register ObjectManager at the inventory root.
        sdbusplus::asio::object_server objectServer(
            connection, true);

        objectServer.add_manager(kInventoryRoot);

        // Publish general inventory information.
        auto item = objectServer.add_interface(
            kChassisPath, kItemInterface);

        item->register_property(
            "PrettyName",
            std::string("Raspberry Pi 4 Chassis"));

        item->register_property("Present", true);

        if (!item->initialize())
        {
            throw std::runtime_error(
                "Failed to initialize Inventory.Item");
        }

        // Identify this inventory item as a chassis.
        auto chassis = objectServer.add_interface(
            kChassisPath, kChassisInterface);

        if (!chassis->initialize())
        {
            throw std::runtime_error(
                "Failed to initialize Inventory.Item.Chassis");
        }

        // Allow systemd to stop the service gracefully.
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);

        signals.async_wait(
            [&](const boost::system::error_code&, int)
            {
                io.stop();
            });

        std::cout << "Chassis inventory service started\n";

        io.run();

        std::cout << "Chassis inventory service stopped\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << "Chassis inventory error: "
                  << error.what() << '\n';

        return 1;
    }

    return 0;
}