import argparse
import multiprocessing
import os
import platform
import shutil
import sys

import sh

from yautil import Subcommand
from yautil.pyshutil import source

root_dir: str = os.path.realpath(os.path.join(__file__, '..', '..', '..'))


class Optee(Subcommand):
    env_caam = {
        "CFG_CRYPTO_DRIVER": "y",
        "CFG_NXP_CAAM": "y",

        
        "CFG_TA_EVALUATION": "y",       
#        "CFG_TA_RANDOMIZED_KEYPAD": "y",  # Use Software AES
#        "CFG_TA_TWO_FACTOR_AUTHENTICATION": "y",
#        "CFG_TA_FMRI_VIEWER": "y",
#        "CFG_TA_FACE_RECOGNITION": "y",
    }

    env_optee = {
        "PLATFORM": "imx-mx6qsabrelite",
        "ARCH": "arm",
        "CFG_BUILT_IN_ARGS": "y",
        "CFG_PAGEABLE_ADDR": "0",
        "CFG_NS_ENTRY_ADDR": "0x10800000",
        "CFG_DT_ADDR": "0x13000000",
        "CFG_DT": "y",
        "CFG_PSCI_ARM32": "y",
        "DEBUG": "y",
        "CFG_TEE_CORE_LOG_LEVEL": "4",
        "CFG_BOOT_SYNC_CPU": "n",
        "CFG_BOOT_SECONDARY_REQUEST": "y",
        "CFG_IMG_ADDR": "0x4bffffe4",
        "CFG_IMG_ENTRY": "0x4c000000",
        "CFG_WITH_VFP": "y"
    }

    env_linux = {
        "ARCH": "arm",
        "CROSS_COMPILE": "arm-linux-gnueabihf-",
    }

    intermediates = os.path.join(root_dir, "build", ".intermediates")

    j: int
    cross_compile: str
    make: callable

    def __build_uboot(self, uboot_dir: str):
        env_uboot = {**os.environ, **self.env_linux}

        self.make('nitrogen6q_defconfig', _env=env_uboot, _cwd=uboot_dir)
        self.make(j=self.j, _env=env_uboot, _cwd=uboot_dir)

    def __build_optee(self, optee_dir: str, uboot_dir: str, out_dir: str):
        env_optee = {**os.environ, **self.env_optee, **self.env_caam, **{"O": self.intermediates}}
        self.make(j=self.j, _env=env_optee, _cwd=optee_dir)

        mkimage = sh.Command(os.path.join(uboot_dir, "tools", "mkimage"))
        mkoimage = mkimage.bake(
            A="arm", O="linux", C="none", a="0x4dffffe4", e="0x4e000000",
            d=os.path.join(self.intermediates, "core", "tee.bin"))
        mkbootscr = mkimage.bake(
            A="arm", O="linux", T="script", C="none", a="0", e="0",
            n="OP-TEE Android Boot Script", d=os.path.join(root_dir, 'optee-oreo-boot.txt'))

        mkoimage(os.path.join(self.intermediates, "oImage"))
        mkbootscr(os.path.join(self.intermediates, "boot.scr"))

        shutil.copy2(os.path.join(self.intermediates, 'oImage'),
                     os.path.join(out_dir, 'sImage'))

    def __build_linux(self, linux_dir: str, out_dir: str):
        env_linux = {**os.environ, **self.env_linux}
        env_linux = source(os.path.join('build', 'envsetup.sh'), cmd='lunch nitrogen6x-eng',
                           _cwd=os.path.join(root_dir, '..', 'aosp-src'),
                           _env=env_linux)
        env_linux = source('setup_env.sh', _cwd=linux_dir, _env=env_linux)

        self.make('boundary_android_secpath_defconfig', _env=env_linux, _cwd=linux_dir)
        self.make(j=self.j, _env=env_linux, _cwd=linux_dir)

        shutil.copy2(os.path.join(linux_dir, 'arch', 'arm', 'boot', 'dts', 'imx6q-sabrelite.dtb'),
                     out_dir)
        shutil.copy2(os.path.join(linux_dir, 'arch', 'arm', 'boot', 'zImage'),
                     out_dir)
        shutil.copy2(os.path.join(linux_dir, 'drivers', 'smc', 'smc_driver.ko'),
                     out_dir)

    def __build_vcrypto_app(self, vcrypto_app_dir: str, out_dir: str):
        self.make(j=self.j, _cwd=vcrypto_app_dir)

        shutil.copy2(os.path.join(vcrypto_app_dir, 'vcrypto-smc-app'),
                     out_dir)

    def build(self, optee_dir: str, linux_dir: str, uboot_dir: str, vcrypto_app_dir: str,
              out_dir: str):
        self.__build_uboot(uboot_dir)
        self.__build_optee(optee_dir, uboot_dir, out_dir)
        self.__build_linux(linux_dir, out_dir)
        # self.__build_vcrypto_app(vcrypto_app_dir, out_dir)

    def build_and_push_square_anim(self, square_anim_dir: str):
        cross_gcc = sh.Command(self.env_linux['CROSS_COMPILE'] + 'gcc')
        cross_gcc('-static', '-march=armv7-a', 'fb_print.c', '-o', 'fbprint',
                  _cwd=os.path.join(square_anim_dir, 'FPSmeasure'))
        # sh.adb('push', 'FPSmeasure' '/data/FPSmeasure',
        #        _cwd=square_anim_dir)

    def on_parser_init(self, parser: argparse.ArgumentParser):
        if platform.system() == 'Darwin':
            host_arch = 'darwin'
        elif platform.system() == 'Linux':
            host_arch = 'linux'
        else:
            print('error: only OSX and Linux are supported')
            quit()

        def_cross_compile = os.path.join(root_dir, 'prebuilts', 'gcc', host_arch + '-x86', 'arm',
                                         'arm-linux-androideabi-4.9', 'bin', 'arm-linux-androideabi-')

        parser.add_argument('-j', type=int, nargs='?', const=multiprocessing.cpu_count(), default=1)
        parser.add_argument('--optee-os', type=str,
                            default=os.path.join(root_dir, 'optee'),
                            help='directory path.')
        parser.add_argument('--linux', type=str,
                            default=os.path.join(root_dir, 'linux-imx6'),
                            help='directory path.')
        parser.add_argument('--uboot', type=str,
                            default=os.path.join(root_dir, 'u-boot-imx6'),
                            help='directory path.')
        parser.add_argument('--vcrypto-app', type=str,
                            default=os.path.join(root_dir, 'vcrypto-smc-app'),
                            help='directory path.')
        parser.add_argument('-o', '--out', type=str,
                            default=os.path.join(root_dir, 'out'),
                            help='directory path.')
        parser.add_argument('--cross-compile', type=str,
                            default=def_cross_compile)
        parser.add_argument('--square-anim', type=str)

    def on_command(self, args):
        optee_os_dir = os.path.realpath(args.optee_os)
        linux_dir = os.path.realpath(args.linux)
        uboot_dir = os.path.realpath(args.uboot)
        vcrypto_app_dir = os.path.realpath(args.vcrypto_app)
        out_dir = os.path.realpath(args.out)

        self.j = args.j
        self.cross_compile = args.cross_compile
        self.make = sh.make.bake(_out=sys.stdout, _err=sys.stderr)

        if not os.path.isdir(optee_os_dir):
            print(optee_os_dir, " does not exist")
        if not os.path.isdir(linux_dir):
            print(linux_dir, " does not exist")
        if not os.path.isdir(uboot_dir):
            print(uboot_dir, " does not exist")
        # if not os.path.isdir(vcrypto_app_dir):
        #     print(vcrypto_app_dir, " does not exist")
        if not os.path.isdir(out_dir):
            os.makedirs(out_dir)

        if not os.path.isdir(self.intermediates):
            os.makedirs(self.intermediates)

        if args.square_anim:
            self.build_and_push_square_anim(args.square_anim)
            quit()

        out_boot_dir = os.path.join(out_dir)
        os.makedirs(out_boot_dir, exist_ok=True)

        try:
            self.build(optee_dir=optee_os_dir,
                       linux_dir=linux_dir,
                       uboot_dir=uboot_dir,
                       vcrypto_app_dir=vcrypto_app_dir,
                       out_dir=out_boot_dir)
        except sh.ErrorReturnCode:
            quit(-1)
