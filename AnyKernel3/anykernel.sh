### AnyKernel3 Ramdisk Mod Script
## osm0sis @ xda-developers
## Customized for Infinix Zero 5G 2023 (X6815C / MT6877 Dimensity 920)

### AnyKernel setup
# global properties
properties() { '
kernel.string=Underground Kernel for Infinix Zero 5G 2023 (X6815C / MT6877)
do.devicecheck=1
do.modules=0
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=X6815C
device.name2=Infinix-X6815C
device.name3=x6815c
device.name4=pearl
device.name5=mt6877
supported.versions=11, 12, 12.0, 13, 13.0, 14, 14.0, 15, 15.0
supported.patchlevels=
supported.vendorpatchlevels=
'; } # end properties

### AnyKernel install
# boot shell variables
BLOCK=boot;
IS_SLOT_DEVICE=auto;
RAMDISK_COMPRESSION=auto;
PATCH_VBMETA_FLAG=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh;

# Display installation banner
ui_print " ";
ui_print "  --> Flashing Underground Kernel for X6815C...";
ui_print "  --> Backing up authentic local ROM ramdisk...";
ui_print "  --> Injecting Linux 4.19.191+ with Novatek sleep/wake hooks...";
ui_print "  --> Applying grounded MT6877 DTB with verified touch node...";

# Unpack local boot partition
dump_boot;

# Repack boot image with new Image.gz and dtb while preserving local ramdisk and fstab
write_boot;

ui_print " ";
ui_print "  [SUCCESS] Underground Kernel installed successfully!";
ui_print "  [NOTE] Compatible with Stock XOS, LineageOS, HyperOS and AOSP.";
ui_print " ";
## end boot install
