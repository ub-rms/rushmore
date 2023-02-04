
# Rushmore

To build, flash and test Rushmore, you need to have following physical devices. 
* **Board**: Nitrogen6q Sabrelite
* **Display**: BD070LIC2 – 7″ Touchscreen Display

Before starting, you need to be able to control the U-Boot console for your board to follow the instructions below.

First, connect your board with a local machine using a serial port cable, and then use **_picocom_** to use a U-Boot console as below (assuming the connected serial port is appeared as **_/dev/ttyUSB0_**): 
```sh
$ sudo picocom -b 115200 -r -l /dev/ttyUSB0
```
**Please make sure that the above connection works before proceeding to the next steps.**

Now, make _Rushmore_ directory and clone this repository as follows.
```
$ mkdir Rushmore
$ cd Rushmore
Rushmore$ git clone --recurse-submodules https://github.com/ub-rms/rushmore.git
```

## Update U-Boot v2018

If a board has a lower version of U-Boot than _v2018.07_, build U-Boot as follows.
```sh
Rushmore$ cd rushmore/u-boot-imx6
rushmore/u-boot-imx6$ export ARCH=arm
rushmore/u-boot-imx6$ export CROSS_COMPILE=arm-linux-gnueabihf-
rushmore/u-boot-imx6$ make nitrogen6q_defconfig
rushmore/u-boot-imx6$ make -j2
```
This should produce the following files in the same directory.
* `u-boot.xxx` (xxx: bin, cfg, cfgout, imx, lds, map, srec, sym)

Next, add tools directory to your `$PATH` (for later steps):
```
u-boot-imx6$ export PATH=$PATH:${PWD}/tools
```

To copy the latest U-Boot image to SD card, mount the SD card (assuming that the mounted path is _${MOUNTED_SDCARD_PATH}_) and run below script:
```sh
u-boot-imx6$ sudo ./copy_upgrade.sh ${MOUNTED_SDCARD_PATH}
```

Plug your SD card to the board and boot up. Then, run the following command:
```sh
Hit any key to stop autoboot: 0
=> run upgradeu
```

After the update, the board will automatically reboot. After the reboot, type the commands below to reset environment variables:
```sh
Hit any key to stop autoboot: 0
=> env default -a
=> savee
```

## Build and Flash Android

Make a directory for AOSP build:
```
u-boot-imx6$ cd ../../..
Rushmore$ mkdir aosp-src
```

Build Android in the created directory and flash it on your SD card according to the following:
https://boundarydevices.com/android-nougat-7-1-1-release-imx6-boards/

Do not use the prebuilt images there, but follow the instructions for building from the source. When running the **_lunch_** command, choose **_"nitrogen6x-eng"_** .
_(In the case of compiling AOSP, after entering the command  `make 2>&1 | tee build.out`, if you get this error `build/core/ninja.mk:151: recipe for target 'ninja_wrapper' failed make: *** [ninja_wrapper] Error 1`, then enter this command before the make command: `export LC_ALL=C`.)_

Also, before flashing on your SD card, edit the flash script as follows.

1. From your Android root directory, open device/boundary/mksdcard.sh.
2. Find "sudo parted".
3. Use the following sizes for parted (it adds 10MB to the boot partition):  
	```
	mkpart boot 0% 30 \\  
	mkpart recovery 30 50 \\  
	mkpart extended 50 52 \\  
	mkpart data 1610 100% \\  
	mkpart system 52 1076 \\  
	mkpart cache 1076 1588 \\  
	mkpart vendor 1588 1598 \\  
	mkpart misc 1598 1608 \\  
	mkpart crypt 1608 1610 \\  
	```
After flashing, Insert the SD card  and boot up Android and make sure it works.

## Build Rushmore

### Linux Kernel and Rushmore Kernel
To build images of Linux kernel (for the normal world) and Rushmore kernel (for the secure world), run the following commands:
```sh
aosp-src$ cd ../rushmore
rushmore$ python3 build/compile.py optee 
```
This should produce the following files in **_out/_**.
* `zImage` (the Linux kernel image that will replace the kernel image in the original AOSP)
* `sImage` (the Rushmore kernel image that will run in the secure world)
* `imx6q-sabrelite.dtb` (the device table of the newly created Linux kernel)
* `smc_driver.ko` (the Rushmore driver to enable SMC calls between the normal world and the secure world)

### Rushmore Library

To build the Rushmore library that app developers can use, run the following commands:
```sh
rushmore$ cd librushmore
librushmore$ make
```
This should produce the following files in the same directory.
* `librushmore.a`
* `librushmore.so` 

Add the current directory to your `$PATH`:
```
librushmore$ export PATH=$PATH:${PWD}
```

### Boot Script
To make U-Boot to boot our Rushmore system correctly, we need to have our own boot script. Run the following commands to build the boot script.
   ```sh
   librushmore$ cd ../bootscript
   rushmore/bootscript$ make
   ```
This should produce the following files in the same directory.
* `rushmore_bootscript.scr`

### Testing Script and Example Images
The following commands allow you to run our test cases.
```sh
rushmore/bootscript$ cd ../test
test$ make
```
This should produce the following files in the same directory.
* `run_test`

Create encrypt images for testing as follows.
```sh
test$ python3 ../rushmore-source/img2rei.py images/randomized_keypad.png -c vcrypto
test$ python3 ../rushmore-source/img2rei.py images/cube_400x400.gif -c chacah20
```
This should produce the following files in the same directory.
* `randomized_keypad_vcrypto.rei` (a single image)
* `cube_400x400_chacha20.rei` (an animated image with 100 frames)

## Flash Rushmore on Nitrogen6q-Sabrelite 

### Copy All Files  - Linux Kernel, Rushmore Kernel, and Boot Script

Mount your SD card's boot partition (you can use a different path):
```sh
$ sudo mount -t ext4 /dev/sda1 /media/sdcard/boot
```
Go to the **_Rushmore/rushmore_** directory. Copy the images, the device table, the Rushmore driver, and the boot script files to your SD card:
```sh
rushmore$ sudo cp out/zImage /media/sdcard/boot/
rushmore$ sudo cp out/sImage /media/sdcard/boot/
rushmore$ sudo cp out/imx6q-sabrelite.dtb /media/sdcard/boot/
rushmore$ sudo cp out/smc_driver.ko /media/sdcard/boot/
rushmore$ sudo cp bootscript/rushmore_bootscript.scr /media/sdcard/boot/
```

Unmount the SD card's boot partition:
 ```
 $ sudo umount /media/sdcard/boot
```

### Configure U-Boot and Reboot
Using the serial console (**_picocom_**), configure U-boot to use the Rushmore boot script:
```sh
=> setenv selinux permissive
=> setenv extbootargs 'mem=952m'
=> setenv loader 'load mmc 1:1'
=> setenv rushmore_boot 'setenv bootdev mmcblk1; ${loader} 0x10008000 rushmore_bootscript.scr && source 10008000'
=> setenv fb_lvds tm070jdhg30:24:68152388,1280,800,5,63,2,39,1,1
=> savee
=> run rushmore_boot
```
(**_fb_lvds_** is a setting for the BD070LIC2 – 7″ Touchscreen Display)

After rebooting, you will see Android up and running. You can also check if all kernel images (both the normal world and the secure world) booted correctly from the serial port output.

## Test

Now. we can test Rushmore with the testing script we built earlier and the example encrypted images.

### Push Testing Script and Example Images
Using **_adb_**, push the testing script and example images to the board.  
```sh
$ adb push test/run_test /data/.
$ adb push test/images/randomized_keypad_vcrypto.rei /data/.
$ adb push test/images/cube_400x400_chacha20.rei /data/.
```

### Insert Rushmore Driver 

On the board (using the serial console), insert the Rushmore driver into the Linux kernel:
```sh
nitrogen6x:/# insmod /boot/smc_driver.ko
```

### Display Example Images Using the Testing Script

Run the testing script and see the help page:
```sh
nitrogen6x:/# cd data
nitrogen6x:/data# ./run_test
```
> Usage:&ensp; run_test&ensp;\<rei file path\>&ensp;[options...]
>  * Options
>  
>     -r &emsp;\<n\> &emsp;&emsp;Repeat test for \<n\> times	     
>     
>     -d &emsp;\<n\> &emsp;&emsp;Show the \<n\> duplicated images
>     
>     -p &ensp;&emsp;&emsp;&emsp;&ensp;&emsp;Prefetch image to memory (default: buffering)
>     

Here are the commands to display images:
```sh
nitrogen6x:/data# ./run_test randomized_keypad_vcrypto.rei
nitrogen6x:/data# ./run_test cube_400x400_chacha20.rei -p -r 5 -d 3
```
* The first command displays a single randomized keypad image encrypted with Visual Crypto.
* The second command displays three animated cube images 5 times encrypted with ChaCha20.

**_At this point, example images should be displayed on the display._**
