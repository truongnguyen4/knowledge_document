
# Build commands
### Build driver modules

#### LA3.0

```sh
export CLANG_PATH=/aosp/kernel/kernel_platform/prebuilts-master/clang/host/linux-x86/clang-r416183c/bin
export KOUT=/aosp/kernel/kernel_platform/out/msm-waipio-parrot-gki/msm-kernel
export DLKM_OBJ=/aosp/vendor/out/target/product/parrot/obj/DLKM_OBJ
```

#### LA4.0

```sh
export CLANG_PATH=/aosp/kernel/kernel_platform/prebuilts/clang/host/linux-x86/clang-r510928/bin
export KOUT=/aosp/vendor/out/target/product/parrot66/obj/DLKM_OBJ/kernel_platform/msm-kernel
export DLKM_OBJ=/aosp/vendor/out/target/product/parrot66/obj/DLKM_OBJ
```

#### export environment variables

```sh
export PATH=$CLANG_PATH:$PATH
export SYMVERS=$DLKM_OBJ/vendor/datalogic/kernel-modules/device-config/drivers/Module.symvers
export MFLAGS="ARCH=arm64 LLVM=1 LLVM_IAS=1 CROSS_COMPILE=aarch64-linux-gnu-"
```

#### Build kernel driver module

```sh
# cd /aosp/kernel/kernel_platform/msm-kernel/drivers/input/keyboard
make -C $KOUT O=$KOUT M=$(pwd) $MFLAGS modules
```

#### Build vendor driver module

```sh
# cd /aosp/vendor/vendor/datalogic/kernel-modules/dl_keypad/drivers
make -C $KOUT O=$KOUT M=$(pwd) $MFLAGS KBUILD_EXTRA_SYMBOLS=$SYMVERS modules
```

#### Clean

```sh
make -C $KOUT O=$KOUT M=$(pwd) clean
```

### Build AOSP modules

| Module | Build Command | Output Directory | Path | Process |
| --- | --- | --- | --- | --- |
| system service | `m services` | system | system/framework/services.jar | system_server |
| native inputflinger service | `m libinputflinger` | system | system/lib64/libinputflinger.so | system_server |
| datalogic-light-hal | `m com.datalogic.hwmanager-service` | vendor | vendor/bin/hw/com.datalogic.hwmanager-service | com.datalogic.hwmanager-service |
| datalogic-events | `m com.datalogic.dlevents-service` | vendor | vendor/bin/hw/com.datalogic.dlevents-service | com.datalogic.dlevents-service |
| qcom-light-hal | `m vendor.qti.hardware.lights.service` | vendor | vendor/bin/hw/vendor.qti.hardware.lights.service | vendor.qti.hardware.lights.service |
| datalogic-light-aidl | `m com.datalogic.hardware.interfaces.light-update-api` <br/>  <br/> `m com.datalogic.hardware.interfaces.light` | vendor | symbols/vendor/lib64/com.datalogic.hardware.interfaces.light-V1-ndk_platform.so | -- |
| system resource | `m framework-res` | system | system/framework/framework-res.apk | -- |
| sepolicy in vendor | `m selinux_policy_nonsystem` | vendor | vendor/etc/selinux/. | -- |
| sepolicy in system | `m selinux_policy_system selinux_policy_system_ext selinux_policy_product` | system | system/etc/selinux/.</br>system_ext/etc/selinux/.</br>product/etc/selinux/. | -- |

### Build partitions

| Partition | Build Command | Inner Super | Path |
| --- | --- | --- | --- |
| vendor | `m vendorimage` | Y | vendor/out/target/product/parrot/vendor.img<br/>vendor/out/target/product/parrot66/vendor.img |
| vendor_dlkm | `m vendor_dlkmimage` | Y | vendor/out/target/product/parrot/vendor_dlkm.img<br/>vendor/out/target/product/parrot66/vendor_dlkm.img |
| system | `m systemimage` | Y | qssi15/out/target/product/qssi/system.img<br/>qssi17/out/target/product/qssi_64/system.img |
| system_ext | `m systemextimage` | Y | qssi15/out/target/product/qssi/system_ext.img<br/>qssi17/out/target/product/qssi_64/system_ext.img |

# SOURCE

### LA3.0
```sh
# gki config
kernel/kernel_platform/msm-kernel/arch/arm64/configs/gki_defconfig 
# vendor config
kernel/kernel_platform/msm-kernel/arch/arm64/configs/vendor/parrot_GKI.config
# device tree
kernel/kernel_platform/qcom/proprietary/devicetree/qcom
# out
kernel/kernel_platform/msm-waipio-parrot-gki/dist/.config
kernel/kernel_platform/msm-waipio-parrot-consolidate/msm-kernel/.config
```

### LA4.0
```sh
# gki config
kernel/kernel_platform/common/arch/arm64/configs/gki_defconfig
# vendor config
kernel/kernel_platform/msm-kernel/arch/arm64/configs/vendor/parrot_perf.config
# included into vendor_dlkm
kernel/kernel_platform/msm-kernel/parrot.bzl
# device tree
kernel/kernel_platform/qcom/opensource/devicetree/qcom/
# out
kernel/kernel_platform/out/msm-kernel-parrot-perf/dist/.config
# merge config
perf = gki_defconfig + parrot_perf.config
consolidate = perf + consolidate.fragment + parrot_consolidate.config
```

# Flash commands

### Flash Partitions

#### Flash physical partitions

```sh
adb reboot bootloader
fastboot flash super super.img
fastboot reboot
```

#### Flash virtual partitions (sub-partitions in dynamic partitions)
```sh
adb reboot fastboot
fastboot flash vendor vendor.img
fastboot flash vendor_dlkm vendor_dlkm.img
fastboot flash system system.img
fastboot flash system_ext system_ext.img
fastboot reboot
```

### Hot test java/cpp/selinux

#### For java/cpp
```sh
adb remount
adb reboot
adb remount
adb push <local_file> <remote_path>
adb shell killall <process_name>
```

#### For selinux
```sh
Q=qssi15/out/target/product/qssi
V=vendor/out/target/product/parrot
qssi15/out/host/linux-x86/bin/secilc \
  $Q/system/etc/selinux/plat_sepolicy.cil -m -M true -G -N -c 30 \
  $Q/system/etc/selinux/mapping/32.0.cil $Q/system/etc/selinux/mapping/32.0.compat.cil \
  $Q/system_ext/etc/selinux/system_ext_sepolicy.cil \
  $Q/system_ext/etc/selinux/mapping/32.0.cil $Q/system_ext/etc/selinux/mapping/32.0.compat.cil \
  $Q/product/etc/selinux/product_sepolicy.cil $Q/product/etc/selinux/mapping/32.0.cil \
  $V/vendor/etc/selinux/plat_pub_versioned.cil $V/vendor/etc/selinux/vendor_sepolicy.cil \
  -o /dev/null -f /dev/null && echo OK
```

#### Re-apply sepolicy (file_contexts)
```sh
adb shell restorecon -R -v /vendor /odm /mnt/vendor /data/vendor 
```

# DUMP INFORMATION

### Device tree

```sh
adb shell cat /proc/device-tree/model
```

```sh
adb shell cat /proc/device-tree/compatible | tr '\0' '\n'
adb pull /sys/firmware/fdt device_tree_blob.dtb
dtc -I dtb -O dts device_tree_blob.dtb -o device_tree_source.dts
cat device_tree_source.dts
```

Source: `kernel/kernel_platform/qcom/proprietary/devicetree/qcom`

### Kernel config

```sh
adb shell zcat /proc/config.gz > .config
adb pull /proc/config.gz && gunzip config.gz
gunzip config.gz
cat config
```


# DEBUG

### Kernel
```sh
mount -t debugfs none /sys/kernel/debug
```

### RTC time
```sh
adb shell cat /sys/class/rtc/rtc0/since_epoch
adb shell 'echo "$(od -An -tu8 /mnt/vendor/persist/time/ats_1 | tr -d " ")"'

# reset time offset
adb shell 'for f in /mnt/vendor/persist/time/ats_* /data/vendor/time/ats_*; do dd if=/dev/zero of=$f bs=8 count=1; done'
adb shell sync
adb reboot

# test: wrong date + wifi
adb shell svc wifi disable
adb shell settings put global auto_time 0
adb shell date 010112002020
adb shell svc wifi enable
adb logcat -d | grep -aE "CTRL-EVENT-EAP|Certificate verification|not yet valid|SSL3 alert|SSID-TEMP-DISABLED"
```

### Wifi

```sh
adb pull /data/misc/keystore/persistent.sqlite
adb pull /data/misc/apexdata/com.android.wifi/WifiConfigStore.xml
```

### Doze mode

```sh
adb shell dumpsys deviceidle force-idle
adb shell dumpsys deviceidle unforce
adb shell dumpsys deviceidle disable
adb shell dumpsys battery reset
adb shell dumpsys deviceidle | grep -iE "state|enable"      # mState=ACTIVE | IDLE
adb shell dumpsys deviceidle whitelist +/-/=<package>
adb shell dumpsys deviceidle whitelist =net.soti.mobicontrol.androidwork
```

### xPCAT / QFIL

```sh
sudo apt install ./QualcommPackageManager3.3.0.131.7.Linux-x86.deb
mono --version
qpm-cli --product-list
qpm-cli --uninstall pcat
qpm-cli --license-activate qud
qpm-cli --license-activate quts
qpm-cli --license-activate pcat
qpm-cli --install qud
qpm-cli --install quts
qpm-cli --install pcat
```

- QPM3: https://qpm.qualcomm.com/#/main/tools/details/QPM3
- PCAT: https://docs.qualcomm.com/doc/80-88500-5/topic/install_product_configuration_assistant_tool_pcat.html

### Alternatives

```sh
sudo update-alternatives --config java
sudo update-alternatives --config python
```

### Brightness (symbols)

```
mPersistBrightnessNitsForDefaultDisplay
mBrightnessLevelsLuxMap
mBrightnessLevelsNits
mNitsToBrightnessSpline
ConvertLinearToGammaFloat()
UpdateSlider() -> animateSliderTo()
```

### SOTI

```
URL:          https://s006167.mobicontrolcloud.com/MobiControl/oauth/logon
Enrollment:   NGKFUZ56
Account:      afw#mobicontrol
Web login:    huynh_thanh / huynh_thanh
Wifi:         Datalogic-RD-001 / Mc2Sol25!!
```

### input (keypad)

```
input_report_key
input_report_rel
input_report_abs
input_report_switch
input_sync
```

```
adb shell insmod /vendor_dlkm/lib/modules/dl_keypad.ko
adb shell insmod /vendor_dlkm/lib/modules/tca8418_keypad.ko
```

```
adb shell 'echo 1 > /sys/class/misc/dl_virtual_keys/refresh_keyboard_from_factory'
```

```
cd /aosp/kernel/kernel_platform/msm-kernel
make O=/aosp/kernel/kernel_platform/out/msm-waipio-parrot-gki/msm-kernel \
     ARCH=arm64 LLVM=1 LLVM_IAS=1 CROSS_COMPILE=aarch64-linux-gnu- \
     M=/aosp/vendor/vendor/datalogic/kernel-modules/dl_keypad/drivers \
     KBUILD_EXTRA_SYMBOLS=/aosp/vendor/out/target/product/parrot/obj/DLKM_OBJ/vendor/datalogic/kernel-modules/device-config/drivers/Module.symvers \
     modules
```


# BEAGLEBONE BLACK

### Build module
```sh
BR=/media/truongnguyen/1tb/learn/buildroot
KDIR=$BR/output/build/linux-6.18.1
export PATH=$BR/output/host/bin:$PATH
make -C "$KDIR" ARCH=arm CROSS_COMPILE=arm-linux- M=${PWD} modules -j$(nproc)

```