import argparse
import os
import ctypes

import matplotlib.pyplot as plt
import numpy as np

from img2rei import ReiFileHeader, ReiFrameHeader


def bytes_to_array(data: bytes, width: int, height: int):
    assert len(data) == width * height * 2

    def bytes_to_pixel(pxl_data: bytes):
        assert len(pxl_data) == 2

        val = int.from_bytes(pxl_data, byteorder='little', signed=False)

        return np.array([(val >> 11)/31, ((val >> 5) & 0x3f)/63, (val & 0x1f)/31])

    def bytes_to_row(row_data: bytes):
        return np.array([*map(bytes_to_pixel, [row_data[i:i+2] for i in range(0, len(row_data), 2)])])

    return np.array([*map(bytes_to_row, [data[i:i+width*2] for i in range(0, len(data), width*2)])])


if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument('img')

    args = parser.parse_args()
    if not os.path.isfile(args.img):
        print('error: cannot find input img {:s}'.format(args.img))
        quit(-1)

    try:
        file_hdr_size = ctypes.sizeof(ReiFileHeader)
        frame_hdr_size = ctypes.sizeof(ReiFrameHeader)
        with open(args.img, 'rb') as f:
            file_hdr = ReiFileHeader.from_buffer_copy(f.read(file_hdr_size))

            if file_hdr.magic != 0x5718:
                raise Exception(f'{args.img} is not an rei file.')

            print('file header')
            print(f'  hdr_len:    {file_hdr.hdr_len}')
            print(f'  ver:        {file_hdr.ver}')
            print(f'  fps:        {file_hdr.fps}')
            print(f'  num_frames: {file_hdr.num_frames}')

            f.seek(file_hdr.hdr_len, 0)

            for i in range(file_hdr.num_frames):
                frame_hdr = ReiFrameHeader.from_buffer_copy(f.read(frame_hdr_size))

                print(f'frame {i}')
                print(f'  width: {frame_hdr.width}')
                print(f'  height: {frame_hdr.height}')
                print(f'  pixel_format: {frame_hdr.pixel_format}')
                print(f'  frame_size: {frame_hdr.frame_size}')
                print(f'  crypto_mode: {frame_hdr.crypto_mode}')
                print(f'  iv_len: {frame_hdr.iv_len}')
                f.seek(frame_hdr.iv_len, 1)

                frame_data = f.read(frame_hdr.frame_size - frame_hdr_size - frame_hdr.iv_len)
                img = bytes_to_array(frame_data, frame_hdr.width, frame_hdr.height)
                assert img.shape[0] == frame_hdr.height
                assert img.shape[1] == frame_hdr.width
                assert img.shape[2] == 3
                plt.imshow(img)
                plt.show()
    except Exception as e:
        print(f'error: {e}')
        quit()