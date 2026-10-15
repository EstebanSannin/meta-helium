# tegra-bootfiles works on the shared L4T tree (no do_unpack, SRC_URI = ""),
# so the Helium BCT files are installed straight from this layer.
HELIUM_BCT_DIR := "${THISDIR}/files"
HELIUM_BCT_FILES = " \
    orin_nx_hdmi_a03-pinmux.dtsi \
    orin_nx_hdmi_a03-gpio-default.dtsi \
    orin_nx_hdmi_a03-padvoltage-default.dtsi \
"
do_install[file-checksums] += "${@' '.join('%s/%s:True' % (d.getVar('HELIUM_BCT_DIR'), f) for f in d.getVar('HELIUM_BCT_FILES').split())}"

# Helium pinmux/GPIO/pad-voltage from the PCB Arts JetPack 36.5.0 package
# (generated from helium_dp.xlsm). They are installed under tegra234-* names
# because image_types_tegra only copies tegra234-*.dts* into the flash package.
# Helium has no carrier-board (CVB) EEPROM, so MB2 must not try to read it.
do_install:append() {
    tf=${D}${datadir}/tegraflash
    # PAC.06 is the fan enable. PCB Arts default it low and raise it from a
    # first-boot script (helium_fan_gpio_config.sh, needs libgpiod); drive it
    # high from MB1 instead so the fan is powered from early boot.
    awk '
        /gpio-output-low = </  { list = "low" }
        /gpio-output-high = </ { list = "high"; print; if (main) { print "\t\t\t\tTEGRA234_MAIN_GPIO(AC, 6)"; moved = 1 }; next }
        /gpio_main_default/    { main = 1 }
        /gpio_aon_default/     { main = 0 }
        list == "low" && main && /TEGRA234_MAIN_GPIO\(AC, 6\)/ { next }
        { print }
        END { if (!moved) exit 1 }
    ' ${HELIUM_BCT_DIR}/orin_nx_hdmi_a03-gpio-default.dtsi > $tf/tegra234-mb1-bct-gpio-p3767-helium.dtsi || \
        bbfatal "could not move PAC.06 to gpio-output-high"
    [ $(grep -c 'TEGRA234_MAIN_GPIO(AC, 6)' $tf/tegra234-mb1-bct-gpio-p3767-helium.dtsi) -eq 1 ] || \
        bbfatal "PAC.06 must appear exactly once in the Helium GPIO dtsi"
    install -m 0644 ${HELIUM_BCT_DIR}/orin_nx_hdmi_a03-padvoltage-default.dtsi $tf/tegra234-mb1-bct-padvoltage-p3767-helium.dtsi
    sed -e 's|#include "./orin_nx_hdmi_a03-gpio-default.dtsi"|#include "./tegra234-mb1-bct-gpio-p3767-helium.dtsi"|' \
        ${HELIUM_BCT_DIR}/orin_nx_hdmi_a03-pinmux.dtsi > $tf/tegra234-mb1-bct-pinmux-p3767-helium.dtsi
    grep -q 'tegra234-mb1-bct-gpio-p3767-helium.dtsi' $tf/tegra234-mb1-bct-pinmux-p3767-helium.dtsi || \
        bbfatal "gpio include not rewritten in Helium pinmux dtsi"

    mb2misc=$tf/${TEGRA_FLASHVAR_MB2BCT_CFG}
    sed -i 's/cvb_eeprom_read_size = <0x100>;/cvb_eeprom_read_size = <0x0>;/' $mb2misc
    grep -q 'cvb_eeprom_read_size = <0x0>;' $mb2misc || bbfatal "cvb_eeprom_read_size not patched in $mb2misc"
}
