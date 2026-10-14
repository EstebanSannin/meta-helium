FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += " \
    file://orin_nx_hdmi_a03-pinmux.dtsi \
    file://orin_nx_hdmi_a03-gpio-default.dtsi \
    file://orin_nx_hdmi_a03-padvoltage-default.dtsi \
"

# Helium pinmux/GPIO/pad-voltage from the PCB Arts JetPack 36.5.0 package
# (generated from helium_dp.xlsm). Helium has no carrier-board (CVB) EEPROM,
# so MB2 must not try to read it.
do_install:append() {
    for f in orin_nx_hdmi_a03-pinmux.dtsi orin_nx_hdmi_a03-gpio-default.dtsi \
             orin_nx_hdmi_a03-padvoltage-default.dtsi; do
        install -m 0644 ${UNPACKDIR}/$f ${D}${datadir}/tegraflash/$f
    done
    mb2misc=${D}${datadir}/tegraflash/${TEGRA_FLASHVAR_MB2BCT_CFG}
    sed -i 's/cvb_eeprom_read_size = <0x100>;/cvb_eeprom_read_size = <0x0>;/' $mb2misc
    grep -q 'cvb_eeprom_read_size = <0x0>;' $mb2misc || bbfatal "cvb_eeprom_read_size not patched in $mb2misc"
}
