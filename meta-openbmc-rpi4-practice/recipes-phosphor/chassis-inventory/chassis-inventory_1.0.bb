SUMMARY = "Raspberry Pi 4 OpenBMC Chassis Inventory"
DESCRIPTION = "Publishes Raspberry Pi chassis inventory through D-Bus"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://main.cpp \
    file://meson.build \
    file://chassis-inventory.service \
    file://chassis-inventory-dbus.conf \
"

S = "${UNPACKDIR}"

DEPENDS = " \
    sdbusplus \
    boost \
"

inherit meson pkgconfig systemd

SYSTEMD_SERVICE:${PN} = "chassis-inventory.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install:append() {
    # Install the systemd service.
    install -d ${D}${systemd_system_unitdir}

    install -m 0644 \
        ${UNPACKDIR}/chassis-inventory.service \
        ${D}${systemd_system_unitdir}/chassis-inventory.service

    # Install the D-Bus system policy.
    install -d ${D}${datadir}/dbus-1/system.d

    install -m 0644 \
        ${UNPACKDIR}/chassis-inventory-dbus.conf \
        ${D}${datadir}/dbus-1/system.d/chassis-inventory-dbus.conf
}

FILES:${PN} += " \
    ${datadir}/dbus-1/system.d/chassis-inventory-dbus.conf \
"