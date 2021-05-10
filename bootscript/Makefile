all: rushmore_bootscript.scr 

rushmore_bootscript.scr: rushmore_bootscript.script
	mkimage -A arm -O linux -T script -C none -a 0 -e 0 -n "IMX Sec/NSec Boot Script" -d rushmore_bootscript.script rushmore_bootscript.scr


clean:
	rm *.scr
