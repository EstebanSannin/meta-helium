SUMMARY = "Stable Ethernet interface names for the Helium"
DESCRIPTION = "systemd .link files naming the ports with Torizon's convention: ethernet0 \
(case label "Ethernet 1", RTL8111, PCIe C8) and ethernet1 ("Ethernet 2", LAN7430, PCIe C9). The kernel ethN names follow \
PCIe probe order and swap between boots."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://10-helium-ethernet0.link file://10-helium-ethernet1.link"

do_install() {
    install -d ${D}${nonarch_libdir}/systemd/network
    install -m 0644 ${UNPACKDIR}/10-helium-ethernet0.link ${UNPACKDIR}/10-helium-ethernet1.link \
        ${D}${nonarch_libdir}/systemd/network/
}

FILES:${PN} = "${nonarch_libdir}/systemd/network"
PACKAGE_ARCH = "${MACHINE_ARCH}"
