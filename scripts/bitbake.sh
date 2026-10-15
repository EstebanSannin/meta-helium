#!/bin/bash
# Run bitbake for the Helium inside the Torizon OS 8 crops container.
#
#   layers/meta-helium/scripts/bitbake.sh torizon-docker
#
# The workspace is the repo checkout that contains layers/meta-helium. On the
# first run (no build-helium/conf yet) the build configuration is generated
# with scripts/setup-build.sh. Set HELIUM_RECONFIGURE=1 to regenerate it.
set -euo pipefail

WS=$(cd "$(dirname "$(readlink -f "$0")")/../../.." && pwd)
CROPS_IMAGE=${CROPS_IMAGE:-torizon/crops:wrynose-8.x.y}

if [ ! -d "$WS/layers/meta-helium" ] || [ ! -e "$WS/setup-environment" ]; then
    echo "ERROR: $WS is not a Torizon workspace with layers/meta-helium" >&2
    exit 1
fi

if [ ! -f "$WS/build-helium/conf/bblayers.conf" ] || [ "${HELIUM_RECONFIGURE:-0}" = 1 ]; then
    docker run --rm -v "$WS":/workdir --workdir=/workdir "$CROPS_IMAGE" \
        bash /workdir/layers/meta-helium/scripts/setup-build.sh
fi

tty_flag=""
[ -t 1 ] && tty_flag="-t"
docker run --rm $tty_flag --name "helium-bb-$$" -v "$WS":/workdir --workdir=/workdir \
    "$CROPS_IMAGE" bash -c "
    export MACHINE=helium-orin-nx DISTRO=common-torizon EULA=1
    source /workdir/layers/openembedded-core/oe-init-build-env /workdir/build-helium >/dev/null 2>&1
    exec bitbake \"\$@\"
  " _ "$@"
