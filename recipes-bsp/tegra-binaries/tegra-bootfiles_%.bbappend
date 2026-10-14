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
# (generated from helium_dp.xlsm). Helium has no carrier-board (CVB) EEPROM,
# so MB2 must not try to read it.
do_install:append() {
    for f in ${HELIUM_BCT_FILES}; do
        install -m 0644 ${HELIUM_BCT_DIR}/$f ${D}${datadir}/tegraflash/$f
    done
    mb2misc=${D}${datadir}/tegraflash/${TEGRA_FLASHVAR_MB2BCT_CFG}
    sed -i 's/cvb_eeprom_read_size = <0x100>;/cvb_eeprom_read_size = <0x0>;/' $mb2misc
    grep -q 'cvb_eeprom_read_size = <0x0>;' $mb2misc || bbfatal "cvb_eeprom_read_size not patched in $mb2misc"
}
