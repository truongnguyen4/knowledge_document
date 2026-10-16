# QCM4490 (parrot) Kernel Build — Overview

Overview notes on how `kernel/kernel_platform` is built: the GKI mixed build, config files, and partitions.

---
## 1. Kernel-related partitions

![Kernel-related partitions and their contents](./resources/kernel_partitions.svg)

| Partition | Contents |
|---|---|
| **boot.img** | Kernel `Image` (core + all `=y` modules) + **generic ramdisk** (first-stage `init`) |
| **vendor_boot.img** | **DTB** + **vendor ramdisk** (**first-stage** `.ko` + `modules.load` + ...) 
| **vendor_dlkm.img** | **Second-stage** (`.ko` + `modules.load` + ...) |
| **dtbo.img** | Device Tree Overlays (`.dtbo`) |

## 2. `DL_USE_PREBUILT_GKI` (`vendor/device/qcom/parrot/BoardConfig.mk`)

| Value | boot.img | 
|---|---|
| `true` (default) | Use Google **boot.img** `vendor/google/gki/boot.img` |
| `false` | Use built **boot.img** (from **common**) |

> With `DL_USE_PREBUILT_GKI=true`, `boot.img` is supplied by Google **GKI**. Consequently, changes to modules configured as **built-in**—which would be included in `boot.img`—do not affect the locally built image.

## 3. Configuration structure & build outputs

![Config files resolved into out/.config and build outputs](./resources/kernel_config_flow.svg)

| File | Role |
|---|---| 
| `common/arch/arm64/configs/gki_defconfig` (❌ No changes allowed) | Builds the **kernel Image (GKI)** |
| `msm-kernel/arch/arm64/configs/gki_defconfig` (❌No changes allowed) | Builds the **kernel Image (GKI)** |
| `msm-kernel/arch/arm64/configs/vendor/parrot_GKI.config` (✅ Changes allowed) | Builds **modules (`.ko`) + DTB/DTBO only** |

## 4. Check commands

### Check on compiled time
| File | Role |
| --- | --- |
| `out/msm-waipio-parrot-gki/gki_kernel/common/.config` | **Full GKI** config |
| `out/msm-waipio-parrot-gki/msm-kernel/.config` | Config used to build **modules + DTB** |
| `.config` | The final resolved config after merging all fragments |
| `out/msm-kernel/modules.list.msm.parrot` | **First-stage** module list|
| `out/msm-kernel/modules.vendor_blocklist.msm.parrot` | Modules shipped in vendor_dlkm.img |


### Check on the device

#### Check config and loaded modules
```bash
# actual running config
adb shell "zcat /proc/config.gz"
# pull the actual running config from the device
adb pull /proc/config.gz .
# decompress the pulled config
gunzip config.gz
# list modules loaded on the device
adb shell "cat /proc/modules"
```

#### Check modules shipped on the device

```bash
# second-stage modules (vendor_dlkm.img, mounted on /vendor)
adb shell "ls /vendor/lib/modules/"
adb shell "cat /vendor/lib/modules/modules.load"
adb shell "cat /vendor/lib/modules/modules.blocklist"
```

```bash
# first-stage modules (vendor_boot ramdisk, kept in the rootfs after boot)
adb shell "ls /lib/modules/"
adb shell "cat /lib/modules/modules.load"
```
