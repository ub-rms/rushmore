# Rushmore Artifact

This repository explains how to evaluate the artifact for our paper: 

_Rushmore: Securely Displaying Static and Animated Images Using TrustZone (MobiSys'21)_

We aim for the following two badges:
- **_Artifacts Evaluated - Functional_**: our system can be built correctly. 
- **_Artifacts Available_**: we make source code publicly available with instructions.

For the _"Artifacts Evaluated - Functional"_ badge, we explain how to build our system below ([Build-Only Instructions](#build-only)).

For the _"Artifacts Available"_ badge, we make our source code available in this repo. The full instructions on how to build, flash, and test our system are available [here](README.md). However, the full instructions require the same board and the display that we use.


<a name="build-only"></a>  
# Build-Only Instructions
 
We provide a VirtualBox VM image (Ubuntu, 8.7GB) containing all required packages, prebuilt AOSP images (Nougat-7.1.1), the source code for Rushmore, and all environmental settings prepared. The AOSP prebuilt images are only necessary for flashing and testing Rushmore when following [the full instructions](README.md).

From [this link](https://drive.google.com/file/d/1UTwoowsseZpnz27dSORehDBXVNfIXwBm/view?usp=sharing), download the VM image and install it using VirtualBox Manager with the following settings:
```
[OS Type] Linux
[Version] Ubuntu (64-bit)
[Memory] 4096 MB (recommended)
[Hard Disk] Check  "Use an existing virtual hard disk file" and use the provided image
``` 

Start up the created machine above and use the following Ubuntu account to login: 
```sh
 ID: rushmore
 PWD: rushmore2021
```

After  installing and booting the VM image, you can go to the root directory of Rushmore as follows.
```sh
$ cd ~/Rushmore
```

The directory has Android 7 (Nougat) prebuilt images (**_~/Rushmore/aosp-src_**) and Rushmore source code (**_~/Rushmore/rushmore_**).

### U-Boot v2018
As the first step to build Rushmore, compile U-Boot with the following commands:
```sh
~/Rushmore$ cd rushmore/u-boot-imx6
u-boot-imx6$ export ARCH=arm
u-boot-imx6$ export CROSS_COMPILE=arm-linux-gnueabihf-
u-boot-imx6$ make nitrogen6q_defconfig
u-boot-imx6$ make -j2
```
This should produce the following files in the same directory.
* `u-boot.xxx` (xxx: bin, cfg, cfgout, imx, lds, map, srec, sym)

<a name="kernel"></a>  
### Linux Kernel and Rushmore Kernel
As the second step, compile the Linux kernel (for the normal world) and the Rushmore kernel (for the secure world) as follows.
```sh
u-boot-imx6$ cd ..
rushmore$ python3 build/compile.py optee 
```
This should produce the following output in ***out/***.
* `zImage` (the Linux kernel image that will replace the kernel image in original AOSP)
* `sImage` (the Rushmore kernel image that will run in the secure world)
* `imx6q-sabrelite.dtb` (the device table of the newly created Linux kernel)
* `smc_driver.ko` (the Rushmore driver to enable SMC calls between the normal world and the secure world)

### Rushmore Library

As the third step, compile the Rushmore library (for the normal world) that app developers can use as follows.

```sh
rushmore$ cd librushmore
librushmore$ make
```
This should produce the following output in the same directory.
* `librushmore.a`
* `librushmore.so` 

### Boot Script
As the last step, build the boot script for U-Boot as follows.
   ```sh
   librushmore$ cd ../bootscript
   rushmore/bootscript$ make
   ```
This should produce the following output in the same directory.
* `rushmore_bootscript.scr`

**_This concludes all the steps to build all required images for Rushmore and the boot script._**
