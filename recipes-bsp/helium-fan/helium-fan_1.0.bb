SUMMARY = "Helium fan enable service"
DESCRIPTION = "Holds the Helium fan power enable GPIO (PAC.06) high. PCB Arts raise it \
from a first-boot script (helium_fan_gpio_config.sh); MB1 also drives it high \
(tegra-bootfiles_%.bbappend), and this service keeps it high once Linux owns the line."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://helium-fan-enable.service"

inherit systemd

SYSTEMD_SERVICE:${PN} = "helium-fan-enable.service"
SYSTEMD_AUTO_ENABLE = "enable"

do_install() {
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${UNPACKDIR}/helium-fan-enable.service ${D}${systemd_system_unitdir}/
}

RDEPENDS:${PN} = "libgpiod-tools"
PACKAGE_ARCH = "${MACHINE_ARCH}"
