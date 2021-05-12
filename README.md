
# Rushmore

This repository has source code of Rushmore system.

A user needs to have following physical devices to build, flash and test to follow the instructions. 
* **Board**: Nitrogen6q Sabrelite
* **Display**: BD070LIC2 – 7″ Touchscreen Display

To control U-Boot console in this instruction, a user needs to connect a serial port cable from a board to the local machine. Then, you can use **_picocom_** to enter a U-Boot console as below (assuming the connected serial port is appeared as **_/dev/ttyUSB0_**): 
```sh
$ sudo picocom -b 115200 -r -l /dev/ttyUSB0
```
**Please make sure that the above connection works before proceeding to next step.**

Then, make _Rushmore_ root directory for building and clone current repository under the root directory :
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
This should produce the following files in **_u-boot-imx/_**.
* `u-boot.xxx` (xxx: bin, cfg, cfgout, imx, lds, map, srec, sym)


Next, add tools directory to a path (for later steps):
```
u-boot-imx6$ export PATH=$PATH:${RUSHMORE_ROOT}/rushmore/u-boot-imx6/tools
```

To copy the latest U-Boot image to SD card, run below script:
```sh
u-boot-imx6$ sudo ./copy_upgrade.sh ${MOUNTED_SDCARD_PATH}
```

Plug your SD card to the board and boot up. Then, run below command:
```sh
Hit any key to stop autoboot: 0
=> run upgradeu
```

After upgrade, the board will automatically reboot. After the reboot, type below to reset environment variables:
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

Build Android on the created directory and flash it on your SD card according to this:
https://boundarydevices.com/android-nougat-7-1-1-release-imx6-boards/

Do not use the prebuilt images there, but follow the instructions for building
from the source. When running **_lunch_** command, choose **_"nitrogen6x-eng"_** .
_(In the case of compiling AOSP, after entering the command  `make 2>&1 | tee build.out`, if you get this error  `build/core/ninja.mk:151: recipe for target 'ninja_wrapper' failed make: *** [ninja_wrapper] Error 1`, then enter this command before the make command:  `export LC_ALL=C`.)_


Also, before flashing on SD card, edit the flash script as follows.

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


## Build Rushmore System

### Linux Kernel and Rushmore Kernel
To build images of Linux kernel (for the normal world) and Rushmore kernel (for the secure world), run below script:
```sh
aosp-src$ cd ../rushmore
rushmore$ python3 build/compile.py optee 
```
This should produce the following files in **_out/_**.
* `zImage` - Linux kernel image that will replace the kernel image in original AOSP
* `sImage` - Rushmore kernel image that will firstly boot
* `imx6q-sabrelite.dtb` - device table of the newly created Linux kernel
* `smc_driver.ko` - Rushmore driver to enable SMC call between NW and SW

### Rushmore Library

To build Rushmore library (NW-normal world) that app developers can use, run below commands:
```sh
rushmore$ cd librushmore
librushmore$ make
```
This should produce the following files in  _**librushmore/**_.
* `librushmore.a`
* `librushmore.so` 

Add current directory to a path:
```
librushmore$ export PATH=$PATH:${RUSHMORE_ROOT}/rushmore/librushmore
```

### Boot Script
To make U-Boot to boot our Rushmore system correctly, we build our own boot script. Type blow to build the boot script:
   ```sh
   librushmore$ cd ../bootscript
   rushmore/bootscript$ make
   ```
This should produce the following files in **_bootscript/_**. 
* `rushmore_bootscript.scr`

### Testing Script and Example Images
Move to a directory that contains a testing script and example image files. 
Then, type below to build a testing script:
```sh
rushmore/bootscript$ cd ../test
test$ make
```
This should produce the following files in **_test/_**. 
* `run_test`

Create Rushmore Encrypt Images (REI) with provided example images:
```sh
test$ python3 ../rushmore-source/img2rei.py images/randomized_keypad.png -c vcrypto
test$ python3 ../rushmore-source/img2rei.py images/cube_400x400.gif -c chacah20
```
This should produce the following files in **_test/_**.
* `randomized_keypad_vcrypto.rei` - single image
* `cube_400x400_chacha20.rei` - animated image (100 frames)



## Flash Rushmore on Nitrogen6q-Sabrelite 

### Copy All Files  - Linux Kernel, Rushmore Kernel, and Boot Script

Mount your SD card's boot partition (you can use a different path):
```sh
$ sudo mount -t ext4 /dev/sda1 /media/sdcard/boot
```
Go to **_Rushmore/rushmore_** directory. Copy images, device table, Rushmore driver, and boot script files to your SD card:
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
In serial console (**_picocom_**), configure U-boot to use Rushmore boot script:
```sh
=> setenv selinux permissive
=> setenv extbootargs 'mem=952m'
=> setenv loader 'load mmc 1:1'
=> setenv rushmore_boot 'setenv bootdev mmcblk1; ${loader} 0x10008000 rushmore_bootscript.scr && source 10008000'
=> setenv fb_lvds tm070jdhg30:24:68152388,1280,800,5,63,2,39,1,1
=> savee
=> run rushmore_boot
```
(**_fb_lvds_** is a setting for BD070LIC2 – 7″ Touchscreen Display)

After rebooting, you will see Android OS booted on the screen. You can also check if all kernel images both NW and SW booted correctly from serial port output.


## Test

Now. we can test flashed Rushmore system with a testing script we built earlier and created Rushmore Encrypted Images (REI). 

### Push Testing Script and Example Images
Using **_adb_** tool, push testing script and example images to the board.  
```sh
$ adb push test/run_test /data/.
$ adb push test/images/randomized_keypad_vcrypto.rei /data/.
$ adb push test/images/cube_400x400_chacha20.rei /data/.
```

### Insert Rushmore Driver 

On the board (serial port console), insert Rushmore driver (module) into the Linux kernel:
```sh
nitrogen6x:/# insmod /boot/smc_driver.ko
```

### Display Example Images Using a Testing Script

Run testing script and see help page:
```sh
nitrogen6x:/# cd data
nitrogen6x:/data# ./run_test -h
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

Here are example commands to display images:
```sh
nitrogen6x:/data# ./run_test randomized_keypad_vcrypto.rei
nitrogen6x:/data# ./run_test cube_400x400_chacha20.rei -r 5 -d 3
```
* Display a single randomized keypad image (Visual Crypto)
* Display three animated cube images 5 times (ChaCha20)

**_Now, example images should be displayed on the display._**
