SUMMARY = "OpenBMC Raspberry Pi CPU Temperature Reader"
DESCRIPTION = "Publishes Raspberry Pi CPU temperature through D-Bus"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://main.cpp \
    file://meson.build \
    file://temperature-reader.service \
    file://temperature-reader-dbus.conf \
"

S = "${UNPACKDIR}"

DEPENDS = " \
    sdbusplus \
    boost \
"

inherit meson pkgconfig systemd

SYSTEMD_SERVICE:${PN} = "temperature-reader.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install:append() {
    # Install systemd service.
    install -d ${D}${systemd_system_unitdir}

    install -m 0644 \
        ${UNPACKDIR}/temperature-reader.service \
        ${D}${systemd_system_unitdir}/temperature-reader.service

    # Install D-Bus system bus policy.
    install -d ${D}${datadir}/dbus-1/system.d

    install -m 0644 \
        ${UNPACKDIR}/temperature-reader-dbus.conf \
        ${D}${datadir}/dbus-1/system.d/temperature-reader-dbus.conf
}

FILES:${PN} += " \
    ${datadir}/dbus-1/system.d/temperature-reader-dbus.conf \
"