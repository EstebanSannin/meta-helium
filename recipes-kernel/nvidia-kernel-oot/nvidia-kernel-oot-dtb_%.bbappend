FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://tegra234-p3768-0000+p3767-0000-helium-overlay.dts \
    file://tegra234-p3767-helium-camera-tpg-overlay.dts \
"

# "1" adds the MAX96724 test pattern camera (see helium-max96724-tpg).
HELIUM_CAMERA_TPG ??= "0"

HELIUM_BASE_DTB = "tegra234-p3768-0000+p3767-0000-nv-super.dtb"
HELIUM_DTB = "tegra234-p3768-0000+p3767-0000-helium.dtb"

# Merge the Helium overlay into the devkit DTB next to the other DTBs in ${B},
# so signing, install and deploy handle it like any NVIDIA DTB. Torizon's UEFI
# boots the DTB from the OSTree deployment, so the carrier changes must be
# part of the DTB itself rather than a plugin-manager overlay.
do_helium_dtb() {
    dtc -@ -I dts -O dtb -o ${B}/tegra234-p3768-0000+p3767-0000-helium-overlay.dtbo \
        ${UNPACKDIR}/tegra234-p3768-0000+p3767-0000-helium-overlay.dts
    overlays=${B}/tegra234-p3768-0000+p3767-0000-helium-overlay.dtbo
    if [ "${HELIUM_CAMERA_TPG}" = "1" ]; then
        dtc -@ -I dts -O dtb -o ${B}/tegra234-p3767-helium-camera-tpg-overlay.dtbo \
            ${UNPACKDIR}/tegra234-p3767-helium-camera-tpg-overlay.dts
        overlays="$overlays ${B}/tegra234-p3767-helium-camera-tpg-overlay.dtbo"
    fi
    fdtoverlay -i ${B}/${HELIUM_BASE_DTB} -o ${B}/${HELIUM_DTB} $overlays
}
do_helium_dtb[vardeps] += "HELIUM_CAMERA_TPG"
do_helium_dtb[dirs] = "${B}"
addtask helium_dtb after do_compile before do_sign_dtbs do_install
