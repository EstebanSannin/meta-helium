/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * The NVIDIA camera headers include <nvidia/conftest.h>, which
 * nvidia-kernel-oot generates at build time but does not install.
 * These are the results it gets for the R39.2.1 linux-noble-nvidia-tegra
 * 6.8 kernel for the macros those headers use; they change the layout of
 * struct tegra_vi_graph_entity and struct camera_common_data, so they must
 * match the tegra_camera.ko build.
 */
#define NV_V4L2_ASYNC_CONNECTION_STRUCT_PRESENT
#define NV_TEGRA_PMC_IO_PAD_POWER_ENABLE_PRESENT
