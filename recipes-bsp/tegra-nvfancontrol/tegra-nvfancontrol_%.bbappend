FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://nvfancontrol_helium.conf"

# Helium fan profile from the PCB Arts JetPack 36.5.0 image (/etc/nvfancontrol.conf):
# open-loop control with an inverted-PWM curve (255 = slowest, 0 = full speed)
# tuned for the Helium fan. NVIDIA's closed-loop devkit profile targets RPMs this
# fan does not reach and ends up stopping it.
do_install:append() {
    install -m 0644 ${UNPACKDIR}/nvfancontrol_helium.conf ${D}${sysconfdir}/nvfancontrol.conf
}
