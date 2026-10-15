SUMMARY = "MAX96724 test pattern source for the PCB Arts Helium camera path"
DESCRIPTION = "tegracam sensor driver that makes the Helium's MAX96724 GMSL2 \
deserializer output its internal video pattern over MIPI CSI, to test the \
CSI/VI capture path without a camera. Needs HELIUM_CAMERA_TPG = \"1\" for \
the device tree nodes."
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

SRC_URI = " \
    file://Makefile \
    file://helium_max96724_tpg.c \
    file://nvidia/conftest.h \
"

S = "${UNPACKDIR}"

inherit module

COMPATIBLE_MACHINE = "(helium-orin-nx)"

# Built against the NVIDIA out-of-tree camera framework (tegracam). The
# kernel-module- name makes module.bbclass pass its Module.symvers.
DEPENDS += "kernel-module-nvidia-kernel-oot"
EXTRA_OEMAKE += "NV_OOT_INCLUDE=${STAGING_INCDIR}/nvidia-kernel-oot"

RPROVIDES:${PN} += "kernel-module-helium-max96724-tpg"
