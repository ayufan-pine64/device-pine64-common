ninja_tulip() {
	(
		. out/env-$TARGET_PRODUCT.sh
		exec prebuilts/ninja/linux-x86/ninja -C "$(gettop)" -f out/build-$TARGET_PRODUCT.ninja "$@"
	)
}

sdcard_image() {
	if [[ $# -ne 1 ]] && [[ $# -ne 2 ]]; then
		echo "Usage: $0 <output-image> [data-size-in-MB]"
		return 1
	fi

  out_gz="$1"
  out="$(dirname "$out_gz")/$(basename "$out_gz" .gz)"

  get_device_dir

  boot0="$(gettop)/device/pine64-common/bootloader/boot0.bin"
  uboot="$(gettop)/device/pine64-common/bootloader/u-boot-with-dtb.bin"
  kernel="$ANDROID_PRODUCT_OUT/kernel"
  ramdisk="$ANDROID_PRODUCT_OUT/ramdisk.img"
  ramdisk_recovery="$ANDROID_PRODUCT_OUT/ramdisk-recovery.img"

  boot0_position=8      # KiB
  uboot_position=19096  # KiB
  part_position=21      # MiB
  boot_size=49          # MiB
  cache_size=768        # MiB
  data_size=${2:-1024}  # MiB
  mbs=$((1024*1024/512)) # MiB to sector

  (
    set -eo pipefail

    echo "Compiling dtbs..."
    make -C "$(gettop)/device/pine64-common/bootloader"

    echo "Create beginning of disk..."
    dd if=/dev/zero bs=1M count=$part_position of="$out" status=none
    dd if="$boot0" conv=notrunc bs=1k seek=$boot0_position of="$out" status=none
    dd if="$uboot" conv=notrunc bs=1k seek=$uboot_position of="$out" status=none

    echo "Create boot file system... (VFAT)"
    dd if=/dev/zero bs=1M count=${boot_size} of="${out}.boot" status=none
    mkfs.vfat -n BOOT "${out}.boot"

    mcopy -v -m -i "${out}.boot" "$kernel" ::
    mcopy -v -m -i "${out}.boot" "$ramdisk" ::
    mcopy -v -m -i "${out}.boot" "$ramdisk_recovery" ::
    mcopy -v -s -m -i "${out}.boot" "$(gettop)/device/pine64-common/bootloader/pine64" ::
    cat <<"EOF" > uEnv.txt
console=ttyS0,115200n8
selinux=permissive
optargs=enforcing=0 cma=384M no_console_suspend
kernel_filename=kernel
initrd_filename=ramdisk.img
hardware=sun50iw1p1

# Uncomment to enable LCD screen
# fdt_filename_prefix=pine64/sun50i-a64-lcd-
EOF

    cat <<"EOF" > boot.script
setenv set_cmdline set bootargs console=${console} ${optargs} androidboot.serialno=${sunxi_serial} androidboot.hardware=${hardware} androidboot.selinux=${selinux} earlyprintk=sunxi-uart,0x01c28000 loglevel=8 root=${root}
run mmcboot
EOF
    mkimage -C none -A arm -T script -d boot.script boot.scr
    mcopy -v -m -i "${out}.boot" "boot.scr" ::
    mcopy -m -i "${out}.boot" "uEnv.txt" ::
    rm -f boot.script boot.scr uEnv.txt

    dd if="${out}.boot" conv=notrunc oflag=append bs=1M of="$out" status=none
    rm -f "${out}.boot"

    echo "Append system..."
    simg2img "$ANDROID_PRODUCT_OUT/system.img" "${out}.system"
    dd if="${out}.system" conv=notrunc oflag=append bs=1M of="$out" status=none
    system_size=$(stat -c%s "${out}.system")
    rm -f "${out}.system"

    echo "Append cache..."
    dd if="/dev/zero" conv=notrunc bs=1M of="${out}.cache" count="$cache_size" status=none
    mkfs.f2fs "${out}.cache"
    dd if="${out}.cache" conv=notrunc oflag=append bs=1M of="$out" status=none
    rm -f "${out}.cache"

    echo "Append data..."
    dd if="/dev/zero" conv=notrunc bs=1M of="${out}.data" count="$data_size" status=none
    mkfs.f2fs "${out}.data"
    dd if="${out}.data" conv=notrunc oflag=append bs=1M of="$out" status=none
    rm -f "${out}.data"

    echo "Partition table..."
    cat <<EOF | sfdisk "$out"
$((part_position*mbs)),$((boot_size*mbs)),6
$(((part_position+boot_size)*mbs)),$((system_size/512)),L
$(((part_position+boot_size)*mbs+system_size/512)),$((cache_size*mbs)),L
$(((part_position+boot_size)*mbs+system_size/512)),$((data_size*mbs)),L
EOF

    # TODO: this is broken, because https://github.com/longsleep/u-boot-pine64
    # doesn't execute sunxi_partition_init
    #
    # echo "Updating fastboot table..."
    # sunxi-nand-part -f a64 "$out" $(((part_position-20)*mbs)) \
    #   "boot $((boot_size*mbs)) 32768" \
    #   "system $((system_size/512)) 32768" \
    #   "cache $((cache_size*mbs)) 32768" \
    #   "data 0 33024"

    size=$(stat -c%s "$out")

    if [[ "$(basename "$out_gz" .gz)" != "$(basename "$out_gz")" ]]; then
      echo "Compressing image..."
      pigz "$out"
      echo "Compressed image: $out (size: $size)."
    else
      echo "Uncompressed image: $out (size: $size)."
    fi
  )
}

tulip_sync() {
  (
    set -xe
    command make -C $ANDROID_BUILD_TOP/device/pine64-common/bootloader
    adb wait-for-device
    if ! adb shell mountpoint /bootloader || ! adb shell touch /bootloader; then
      adb shell umount /bootloader || true
      adb shell mount -t vfat /dev/block/mmcblk0p1 /bootloader
    fi
    adb remount
    adb sync system
    for i in kernel ramdisk.img ramdisk-recovery.img; do
      adb push $ANDROID_PRODUCT_OUT/$i /bootloader/
    done
    for i in pine64/sun50i-a64-pine64-plus.dtb pine64/sun50i-a64-lcd-pine64-plus.dtb; do
      adb push $ANDROID_BUILD_TOP/device/pine64-common/bootloader/$i /bootloader/$i
    done
    adb shell sync
  )
}
