#!/usr/bin/env python3

import argparse
import os
import secrets
from enum import Enum
from typing import List
import ctypes
import imageio as imageio
from tqdm import tqdm
from Crypto.Cipher import AES, ChaCha20
import numpy as np


__key = bytes([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
               0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
               0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
               0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f])

enable_frame_limit = True
num_frame_limit = 100

class CryptoMode(Enum):
    AES_MODE = 'aes'
    CHACHA20_MODE = 'chacha20'
    VCRYPTO_MODE = 'vcrypto'

    def __str__(self):
        return self.value

    # These numbers MUST match with 'enum crypto_mode' in
    # optee/core/arch/arm/plat-imx/ta/include/rushmore_tee_api.h
    def __int__(self):
        if self == self.AES_MODE:
            return 1
        elif self == self.CHACHA20_MODE:
            return 2
        elif self == self.VCRYPTO_MODE:
            return 4
        else:
            raise ValueError('')


# Rushmore Encrypted Image format

# REI file structure
#
# +-------+-------+-------+-------+
# |        REI file header        |
# +-------+-------+-------+-------+
# |        REI frame header       |
# +-------+-------+-------+-------+
# |           frame_data          |
# |              ...              |
# +-------+-------+-------+-------+
# |        REI frame header       |
# +-------+-------+-------+-------+
# |           frame_data          |
# |              ...              |
# +-------+-------+-------+-------+
# |        REI frame header       |
# +-------+-------+-------+-------+
# |           frame_data          |
# |              ...              |
# +-------+-------+-------+-------+
# |                               |
# |              ...              |
# |                               |
# +-------+-------+-------+-------+


#
# REI file header
#
# 0   4   8       16             31-th bit
# +-------+-------+-------+-------+
# | magic(0x5718) |    hdr_len    |
# +-------+-------+-------+-------+
# |  ver  |   -   |      fps      |
# +-------+-------+-------+-------+
# |           num_frames          |
# +-------+-------+-------+-------+
#
class ReiFileHeader(ctypes.BigEndianStructure):
    _pack_ = 1
    _fields_ = [
        ('magic', ctypes.c_uint16),
        ('hdr_len', ctypes.c_uint16),
        ('ver', ctypes.c_uint8),
        ('reserved', ctypes.c_uint8),
        ('fps', ctypes.c_uint16),
        ('num_frames', ctypes.c_uint32),
    ]


# REI frame header
#
# 0   4   8       16             31-th bit
# +-------+-------+-------+-------+
# |     width     |     height    |
# +-------+-------+-------+-------+
# |  pixel_format |    reserved   |
# +-------+-------+-------+-------+
# |           frame_size          |
# +-------+-------+-------+-------+
# |  crypto_mode  |     iv_len    |
# +-------+-------+-------+-------+
# |              iv               |
# |              ...              |
# +-------+-------+-------+-------+
class ReiFrameHeader(ctypes.BigEndianStructure):
    _pack_ = 1
    _fields_ = [
        ('width', ctypes.c_uint16),
        ('height', ctypes.c_uint16),
        ('pixel_format', ctypes.c_uint16),
        ('reserved', ctypes.c_uint16),
        ('frame_size', ctypes.c_uint32),
        ('crypto_mode', ctypes.c_uint16),
        ('iv_len', ctypes.c_uint16),
    ]


def encrypt_aes(plain: bytes, key: bytes, iv: bytes) -> bytes:
    aes = AES.new(key, AES.MODE_CBC, iv)
    return aes.encrypt(plain)


def decrypt_aes(cipher: bytes, key: bytes, iv: bytes) -> bytes:
    aes = AES.new(key, AES.MODE_CBC, iv)
    return aes.decrypt(cipher)


def encrypt_chacha20(plain: bytes, key: bytes, nonce: bytes) -> bytes:
    chacha = ChaCha20.new(key=key, nonce=nonce)
    return chacha.encrypt(plain)


def array_to_halftone_bytes(frame: np.array) -> bytes:
    ret = bytes()
    median = np.median(frame)
    frame = frame.tolist()

    for row in frame:
        for pixel in row:
            grey = sum(pixel) / len(pixel)

            ret += (0xffff if grey >= median else 0x0000).to_bytes(2, byteorder='big', signed=False)

    return ret


def encrypt_vcrypto(data: bytes, key: bytes, nonce: bytes, width: int, height: int) -> bytes:
    assert len(data) % (width *2) == 0
    assert height % 2 == 0

    keystream = encrypt_chacha20(bytes(int((len(data) / 8 / 2)) + 1), key=key, nonce=nonce)

    pos = 0
    out = bytearray(width * height * 2)

    # for i in range(32):
    #     print(hex(int.from_bytes(keystream[i * 4:i * 4 + 4], byteorder='big', signed=False)))

    for keybyte in keystream:
        # if pos < 8 * 16:
        #     print('0x{:02X}'.format(keybyte), end=' ')
        for i in range(8):
            if pos >= len(out):
                # for i in range(72):
                #     if i % 8 == 0:
                #         print()
                #     print('0x{:04x}'.format(int.from_bytes(out[i * 2:i * 2 + 2], byteorder='big', signed=False)), end=' ')
                # print()
                return bytes(out)
            # print('x: {:d}, y: {:d}'.format(int((pos % (width * 2)) / 2), int(pos / (width * 2))))
            key_zero = (keybyte & (0x1 << i)) == 0
            data_zero = data[pos] == 0
            # data_zero = False
            # xor
            if data_zero == key_zero:
                out[pos] = out[pos + 1] = 0x00
                out[pos + width * 2] = out[pos + width * 2 + 1] = 0xff
            else:
                out[pos] = out[pos + 1] = 0xff
                out[pos + width * 2] = out[pos + width * 2 + 1] = 0x00
            pos += 2

            if pos % (width * 2) == 0:
                # if pos reaches the end of a row skip a row
                pos += width * 2

        # if pos < 8 * 16:
        #     print('0x{:04x} 0x{:04x} 0x{:04x} 0x{:04x} 0x{:04x} 0x{:04x} 0x{:04x} 0x{:04x}'.format(
        #         int.from_bytes(out[pos - 16: pos - 14], byteorder='big', signed=False),
        #         int.from_bytes(out[pos - 14: pos - 12], byteorder='big', signed=False),
        #         int.from_bytes(out[pos - 12: pos - 10], byteorder='big', signed=False),
        #         int.from_bytes(out[pos - 10: pos - 8], byteorder='big', signed=False),
        #         int.from_bytes(out[pos - 8: pos - 6], byteorder='big', signed=False),
        #         int.from_bytes(out[pos - 6: pos - 4], byteorder='big', signed=False),
        #         int.from_bytes(out[pos - 4: pos - 2], byteorder='big', signed=False),
        #         int.from_bytes(out[pos - 2: pos], byteorder='big', signed=False),
        #     ))

    out = bytes(out)

    # for i in range(32):
    #     print(i)
    #     print(hex(int.from_bytes(out[i * 2:i * 2 + 2], byteorder='big', signed=False)), end=' ')

    return out


def pixel_to_bytes(pixel: List) -> bytes:
    return ((((int(pixel[0]) >> 3) << 11) & 0xf800)
            | (((int(pixel[1]) >> 2) << 5) & 0x07e0)
            | ((int(pixel[2]) >> 3) & 0x001f)).to_bytes(2, byteorder='little', signed=False)

    # B only
    # return ((((pixel[2] >> 3) << 11) & 0xf800)).to_bytes(2, byteorder='big', signed=False)
    # R only
    # return ((((pixel[0] >> 2) << 5) & 0x07e0)).to_bytes(2, byteorder='big', signed=False)
    # G only
    # return (((pixel[1] >> 3) & 0x001f)).to_bytes(2, byteorder='big', signed=False)


def array_to_bytes(frame: np.array) -> bytes:
    frame = frame.tolist()

    ret = bytes()
    for row in frame:
        for pixel in row:
            ret += pixel_to_bytes(pixel)
    return ret


def enc_frame(mode: CryptoMode, key: bytes, frame: np.array) -> bytes:
    height, width, depth = frame.shape

    # Skipped encryption
    b = array_to_bytes(frame)
    iv = secrets.token_bytes(12)

    hdr = ReiFrameHeader()
    hdr.width = width
    hdr.height = height
    hdr.pixel_format = 1
    hdr.frame_size = ctypes.sizeof(hdr) + len(iv) + len(b)
    hdr.crypto_mode = int(mode)
    hdr.iv_len = len(iv)

    ret = bytes(hdr) + iv + b

    assert len(ret) == hdr.frame_size

    return ret


if __name__ == '__main__':
    print('Please use img2rei.py with "-c none" option instead of this.')
    quit()

    parser = argparse.ArgumentParser()

    parser.add_argument('img')
    parser.add_argument('-o', '--output', type=str,
                        help='output file name. (default: <img>_<crypto>.rei)')

    parser.add_argument('--screen-width', type=int,
                        default=1280)
    parser.add_argument('--screen-height', type=int,
                        default=800)

    args = parser.parse_args()
    if not os.path.isfile(args.img):
        print('error: cannot find input img {:s}'.format(args.img))
        quit(-1)
    if not args.output:
        args.output = '{:s}.roi'.format(str(args.img).rsplit(sep='.', maxsplit=1)[0])

    if not (img := imageio.get_reader(args.img)):
        print('error: {:s} is not a valid image file'.format(args.img))
        quit(-1)

    hdr = ReiFileHeader()
    hdr.magic = 0x5718
    hdr.hdr_len = ctypes.sizeof(hdr)
    hdr.ver = 1
    hdr.fps = 0
    hdr.num_frames = num_frame_limit if enable_frame_limit else len(img)

    out = bytes(hdr)

    # with MpUtil(total=len(img), ordered=True, processes=1) as mpctx:
    #     for i, frame in enumerate(img):
    #         height, width, depth = frame.shape
    #         mpctx.schedule(enc_frame, i, height, width, frame)
    #     results = mpctx.wait()

    for i, frame in tqdm(enumerate(img), total=len(img), desc='frames'):
        if enable_frame_limit and num_frame_limit == i:
            break

        height, width, depth = frame.shape
        if (width > args.screen_width) or (height > args.screen_height):
            print('warning: skip a frame ({:d}x{:d}) larger than the screen ({:d}x{:d})'.format(
                width, height, args.screen_width, args.screen_height))
            hdr.num_frames -= 1
            continue
        out += enc_frame(4, __key, frame)

    if hdr.num_frames <= 0:
        print('error: no frames are converted')
        quit(-1)

    with open(args.output, 'wb+') as f:
        f.write(out)

    print('saved {:s}'.format(args.output))
