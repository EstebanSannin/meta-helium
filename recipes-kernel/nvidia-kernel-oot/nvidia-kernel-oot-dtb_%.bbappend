FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://tegra234-p3768-0000+p3767-0000-helium-overlay.dts"

HELIUM_BASE_DTB = "tegra234-p3768-0000+p3767-0000-nv-super.dtb"
HELIUM_DTB = "tegra234-p3768-0000+p3767-0000-helium.dtb"

# Merge the Helium overlay into the devkit DTB next to the other DTBs in ${B},
# so signing, install and deploy handle it like any NVIDIA DTB. Torizon's UEFI
# boots the DTB from the OSTree deployment, so the carrier changes must be
# part of the DTB itself rather than a plugin-manager overlay.
do_helium_dtb() {
    dtc -@ -I dts -O dtb -o ${B}/tegra234-p3768-0000+p3767-0000-helium-overlay.dtbo \
        ${UNPACKDIR}/tegra234-p3768-0000+p3767-0000-helium-overlay.dts
    fdtoverlay -i ${B}/${HELIUM_BASE_DTB} -o ${B}/${HELIUM_DTB} \
        ${B}/tegra234-p3768-0000+p3767-0000-helium-overlay.dtbo
}
do_helium_dtb[dirs] = "${B}"
addtask helium_dtb after do_compile before do_sign_dtbs do_install
