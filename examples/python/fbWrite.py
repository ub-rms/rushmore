'''
    Author: Chang Min park
    Last Updated: 06/05/19
'''
import linuxfb as FB
import sys, os, argparse

parser = argparse.ArgumentParser(description="Write on /dev/fb0 device", formatter_class=argparse.RawTextHelpFormatter)
parser.add_argument('-x', type=str, help='Width of the frame', default='0')
parser.add_argument('-y', type=str, help='Height of the frame', default='0')
parser.add_argument('-f', type=str, help='File containing the raw frame', default='')
args = parser.parse_args()

WIDTH = int(args.x)
HEIGHT = int(args.y)
FILE = args.f
FB_PATH = '/dev/fb0'

# Read raw frames from a file 
def drawFramesFromFile():
    fb_mem = FB.open_fbmem()
    f = open(FB_PATH, 'wb')

    # This offset is configurable
    x_offset = 800
    y_offset = 200

    with open(FILE, "rb") as fFrame:
        f.seek(((x_offset)+(y_offset*fb_mem.var_info.xres))*4, os.SEEK_CUR)
        for h in range(0, HEIGHT):
            for w in range(0, WIDTH):
                f.write(fFrame.read(1))     # Red
                f.write(fFrame.read(1))     # Green
                f.write(fFrame.read(1))     # Blue
                f.write((0).to_bytes(1, byteorder='little'))    # Alpha
            f.seek((fb_mem.var_info.xres-WIDTH)*4, os.SEEK_CUR)

    f.close()
    FB.close_fbmem(fb_mem)


# This method fills the whole display with one color (for easy checking)
def drawWithOneColor(r, g, b):
    fb_mem = FB.open_fbmem()
    f = open(FB_PATH, 'wb')
    for x in range(0, fb_mem.var_info.xres):
        for y in range(0, fb_mem.var_info.yres):
            f.write(r.to_bytes(1, byteorder='little'))
            f.write(g.to_bytes(1, byteorder='little'))
            f.write(b.to_bytes(1, byteorder='little'))
            f.write((0).to_bytes(1, byteorder='little'))

    f.close()
    FB.close_fbmem(fb_mem)


def main():
    if len(sys.argv) < 2:
        drawWithOneColor(255, 0, 255)

    elif WIDTH != 0 and HEIGHT != 0 and FILE != '':
        drawFramesFromFile()

    else:
        raise Exception('Incorrect arguments - please check proper usage (-h)')


if __name__ == "__main__":
    main()
