#!/bin/bash
# Generate the build-helium configuration. Runs inside the Torizon crops
# container (scripts/bitbake.sh calls it); the workspace is mounted at /workdir.
#
# Torizon's setup-environment only accepts machines listed in
# meta-torizon-bsp/scripts/lib/machines.conf, so the config is generated for
# the closest stock machine (Jetson Orin NVMe devkit: same vendor, same layers)
# and MACHINE is switched to helium-orin-nx afterwards. No Torizon file is
# modified; everything happens in build-helium/conf.
set -e
cd /workdir
export MACHINE=jetson-orin-nano-devkit-nvme DISTRO=common-torizon EULA=1
. ./setup-environment build-helium >/dev/null

sed -i 's/^MACHINE ?= .*/MACHINE ?= "helium-orin-nx"/' conf/auto.conf
grep -q meta-helium conf/bblayers.conf || \
  sed -i 's|^EXTRALAYERS ?= ""|EXTRALAYERS ?= "${OEROOT}/layers/meta-helium"|' conf/bblayers.conf

# setup-environment-nvidia lists meta-security in both BASELAYERS and
# BSPLAYERS, which makes bitbake fail with "Found duplicated
# BBFILE_COLLECTIONS 'security'". Drop the BSPLAYERS copy.
python3 - conf/bblayers.conf <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read()
head, sep, rest = s.partition('BSPLAYERS ?=')
block, end, tail = rest.partition('"\n')
open(p, 'w').write(head + sep + block.replace('  ${OEROOT}/layers/meta-security \\\n', '', 1) + end + tail)
PYEOF

echo "build-helium configured: MACHINE=helium-orin-nx DISTRO=common-torizon"
