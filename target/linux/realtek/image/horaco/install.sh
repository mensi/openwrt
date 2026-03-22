#!/bin/sh
# Horaco OpenWrt Factory Installation Script
# This script is executed by the vendor 'imi' binary via the patch.tar.gz mechanism.

# Redirect output to console for visibility during serial boot
exec > /dev/console 2>&1

echo "---------------------------------------------------------------"
echo " Starting Horaco OpenWrt Factory Install..."
echo "---------------------------------------------------------------"

# 1. Validation: Ensure we are running on the expected hardware
if [ ! -f /mnt/mac.txt ]; then
    echo "ERROR: /mnt/mac.txt not found. This does not look like the vendor environment."
    exit 1
fi

# 2. Extract and format MAC address
# Vendor format: aabb.ccdd.eeff
# Target format: aa:bb:cc:dd:ee:ff
RAW_MAC=$(cat /mnt/mac.txt)
if [ -z "$RAW_MAC" ]; then
    echo "ERROR: MAC address file is empty."
    exit 1
fi

# Use sh built-in parameter expansion to avoid dependency on awk/sed
# Split by dots: aabb.ccdd.eeff -> p1=aabb, p2=ccdd, p3=eeff
p1="${RAW_MAC%%.*}"
p2_p3="${RAW_MAC#*.}"
p2="${p2_p3%.*}"
p3="${p2_p3#*.}"

# Split 4-char strings into 2-char octets
m1="${p1%??}" ; m2="${p1#??}"
m3="${p2%??}" ; m4="${p2#??}"
m5="${p3%??}" ; m6="${p3#??}"

MAC_FMT="$m1:$m2:$m3:$m4:$m5:$m6"

echo "Found Hardware MAC: $MAC_FMT"

# 3. Update U-Boot Environment
if [ -f /home/patch/setmac ]; then
    echo "Updating U-Boot environment (ethaddr)..."
    chmod +x /home/patch/setmac
    /home/patch/setmac /dev/mtd1 "$MAC_FMT"
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to update U-Boot environment."
        exit 1
    fi
else
    echo "ERROR: setmac utility missing."
    exit 1
fi

# 4. Flash OpenWrt Image
# We flash to /dev/mtd3 which corresponds to the main firmware area (0x100000 - 0x2000000).
# RUNTIME1 starts at 0x300000, so we seek 2MB (32 * 64KB blocks).
IMAGE="/home/patch/openwrt.uImage"
if [ -f "$IMAGE" ]; then
    echo "Flashing OpenWrt to RUNTIME partitions (MTD3 offset 2MB)..."
    # Sync before and after to ensure stability
    sync
    dd if="$IMAGE" of=/dev/mtd3 bs=64k seek=32
    if [ $? -ne 0 ]; then
        echo "ERROR: Flash operation failed!"
        exit 1
    fi
    sync
else
    echo "ERROR: OpenWrt image missing."
    exit 1
fi

echo "---------------------------------------------------------------"
echo " Install complete! Rebooting into OpenWrt..."
echo "---------------------------------------------------------------"

# Wait a second for buffers to flush
sleep 1
reboot
