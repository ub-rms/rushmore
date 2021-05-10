# Trusted Image/Video Rendring Project

## Build

TBD, sorry!


## Rushmore Encrypted Image (REI)

Rushmore Encrypted Image (REI) format is an encrypted image / animation format for Rushmore framework. Any images or GIF animations have to be converted into this format before it's securely rendered with Rushmore.

You can find detailed file format description at `/img2rei.py`.

> @ChangMin, REI file format has a 16-bit field for recommended fps but it's not used for now. You might want to set the field with the similar field in GIF header and use the value as default fps unless it's overridden by user application (e.g., you might want to add a new fps option, let's say, `--fps` to `run_evaluation`.)

> @ChangMin, `img2rei.py` can support any type of input files that `imageio` python package supports. [`imageio` says it can support even video formats](https://imageio.readthedocs.io/en/stable/formats.html) (!!) but I have not checked yet.

### Walkthrough: Playing REI

> [IMPORTANT] We assume you already know how to build this project.
> This walkthrough is to explain how to convert regular img / gif animation files into REI files and securely render them with Rushmore.

1. Prepare an image file, let's say, `cat.gif`. Be aware that the image file's width ***MUST*** be the multiples of 4. Any other files may cause a kernel crash with memory alignment issues when using SIMD-enabled cryptos (e.g., chacha20, vcrypto). Sorry, this should better be fixed later.

2. Convert the image, `cat.gif`, into `cat.rei`. You should be able to find a converting Python script, `img2rei.py`, at the root of this repository. You can (and probably have to) specify crypto to use with `-c` option. For example,
``` bash
$ python3 img2rei.py cat.gif -c chacha20
```

3. Compile optee, linux, librushmore, and your application (run_evaluation in this example.)
    1. You'd know how to compile optee and linux.
    2. **librushmore:** Go to `/librushmore` and `make`.
    3. **run_evaluation:** Go to [`<rushmore-experiment>/evaluation/source/Evaluation`](https://github.com/ub-rms/rushmore-experiments/tree/dhkim09%40kaist.ac.kr/develop/evaluation/source/Evaluation). `dhkim09a@kaist.ac.kr/develop` branch would work. Export `LIBRUSHMORE` env. variable to let Make know where to find `librushmore.a`. You probably want to do `LIBRUSHMORE=<rushmore_soruce>/librushmore`. And `make`.

4. Burn SDCard with new optee & linux image and boot the board.

5. Upload your app, `run_evaluation`, and your rei file, `cat.rei` to the board.

6. Execute it! You ***MUST*** specify `-p` option for `run_evaluation`. `-p` is for prefetching the image file on memory and current `run_evaluation` only supports prefetched execution. FYI, you can duplicate the image on the screen or automatically repeat image rendering with `-d` and `-r` options, respectively.
``` bash
# ./run_evaluation /somewhere/cat.rei -p
```

## Develop
* Branch Name Rule: {UBIT Name}@buffalo.edu/develop (e.g., cpark22@buffalo.edu/develop)
