import argparse
import glob
import os
import re
import signal
import sys
import tempfile
from enum import Enum

import sh
from tqdm import tqdm

from yautil import Subcommand, SubcommandParser


class UploadTarget(Enum):
    FPSmeasure = 'evaluation/source/SquareAnim/FPSmeasure'
    Evaluation = 'evaluation/source/Evaluation'

    def __str__(self):
        return self.value


class Upload(Subcommand):
    def on_parser_init(self, parser: argparse.ArgumentParser):
        parser.add_argument('rushmore_experiment')
        parser.add_argument('experiment',
                            choices=[e.name for e in UploadTarget])
        parser.add_argument('--full', action='store_true')

    def on_command(self, args):
        rsync = sh.rsync.bake(r=True, p=True, t=True, z=True, L=True, info='progress2', no_inc_recursive=True)
        if not args.full:
            rsync = rsync.bake(exclude='venv')
            rsync = rsync.bake(exclude='*frames')

        tmpdir = tempfile.TemporaryDirectory()

        relpath = str(UploadTarget[args.experiment])

        os.makedirs(tmpdir.name, exist_ok=True)

        rsync(os.path.join(args.rushmore_experiment, str(relpath)), tmpdir.name + '/', _out=sys.stdout)

        print('pushing ' + str(relpath) + '...', end='', flush=True)
        sh.adb.push(*glob.glob(os.path.join(tmpdir.name, '*')), '/data/')
        print('done')


class SquareAnim(Subcommand):

    def on_parser_init(self, parser: argparse.ArgumentParser):
        parser.add_argument('-t', '--test', type=str, required=True,
                            choices=['encrypted_square_anim',
                                     'chacha20_square_anim',
                                     'vcrypto_bw_square_anim',
                                     'run_evaluation'])
        parser.add_argument('-s', '--size', action='append', type=int,
                            choices=[200, 400, 600, 800], required=True)
        parser.add_argument('-r', '--repeat', type=int,
                            default=1)
        parser.add_argument('--fps', type=int)

    def on_command(self, args):
        exp = sh.adb.shell.bake('cd /data/FPSmeasure && ')

        if sh.adb.shell.lsmod().find('smc_driver') < 0:
            sh.adb.shell.insmod('/boot/smc_driver.ko')

        for size in args.size:
            cum_delay_ms = 0
            nw = sw = 0
            for i in tqdm(range(args.repeat), desc='repeats'):
                res = exp('./' + args.test, size, size, 1, args.fps)

                m = re.search(r'before, (?P<sec>\d+), (?P<nsec>\d+)', str(res))
                before = int(m.group('sec')) * 1000000000 + int(m.group('nsec'))
                m = re.search(r'after, (?P<sec>\d+), (?P<nsec>\d+)', str(res))
                after = int(m.group('sec')) * 1000000000 + int(m.group('nsec'))
                cum_delay_ms += (after - before) / 1000000

                m = re.search(r'normal world rendering us: (?P<usec>\d+)', str(res))
                nw += int(m.group('usec')) / 1000 / 100
                m = re.search(r'secure world rendering us: (?P<usec>\d+)', str(res))
                sw += int(m.group('usec')) / 1000 / 100

            if args.repeat > 1:
                print('avg. ', end='')
            print('delay: ' + '{:.3f}'.format(cum_delay_ms / args.repeat) + ' ms'
                  ' (= {:.3f}'.format(args.repeat * 100 / (cum_delay_ms / 1000)) + ' fps)')
            print('avg. normal world rendering: {:.3f} ms'.format(nw))
            print('avg. secure world rendering: {:.3f} ms'.format(sw))


class Evaluation(Subcommand):

    def on_parser_init(self, parser: argparse.ArgumentParser):
        parser.add_argument('-t', '--test', type=str,
                            choices=['run_evaluation'],
                            default='run_evaluation')
        parser.add_argument('--eval-dir', type=str,
                            default='/data/Evaluation')
        parser.add_argument('-d', '--dup', type=int,
                            default=1,
                            help='number of image duplications')
        parser.add_argument('-r', '--repeat', type=int,
                            default=1,
                            help='number of repetition')
        parser.add_argument('-n', '--force-nw', action='store_true',
                            default=False,
                            help='force normal world rendering for testing purpose')
        parser.add_argument('-v', '--verbose', action='store_true',
                            default=False,
                            help='print raw outputs')
        parser.add_argument('-l', '--limit-fps', action='store_true',
                            default=False,
                            help='limit fps to 30')

        input_args = parser.add_mutually_exclusive_group()
        input_args.add_argument('file', type=str, nargs='?')
        # input_args.add_argument('-c', type=str,
        #                     choices=['aes', 'chacha20', 'vcrypto'],
        #                     default='aes')

    @staticmethod
    def handler(signum, frame):
        sh.adb.shell.killall('./run_evaluation')

    def on_command(self, args):
        rc: sh.RunningCommand

        signal.signal(signal.SIGINT, self.handler)
        exp = sh.adb.shell.bake(fr'cd {args.eval_dir} && ./{args.test}',
                                _bg=True, _bg_exc=False)
        if args.verbose:
            exp = exp.bake(_out=sys.stdout, _err=sys.stderr, _tee='out')

        if sh.adb.shell.lsmod().find('smc_driver') < 0:
            sh.adb.shell.insmod('/boot/smc_driver.ko')

        try:
            # basename = 'cat_gif'
            # basename = 'cube_parts_400x400'
            rc = exp(
                args.file,
                # fr'{basename}_{args.c}.rei',
                p=True,
                d=args.dup,
                n=args.force_nw,
                r=args.repeat,
                l=args.limit_fps,
            )
            rc.wait()
        except sh.ErrorReturnCode_143:
            # killed by user
            pass
        except Exception as e:
            print(e)

        # Split stdout of each run
        souts = rc.stdout.decode(sh.DEFAULT_ENCODING).split(r'[Run]')

        if len(souts) <= 1:
            return

        # Before the first run, num_frames is printed
        if m := re.search(r'num_frames received: (?P<num_frames>\d+)', souts[0]):
            num_frames = int(m['num_frames'])
        else:
            print('error: failed to get the number of frames per run from stdout. use 1')
            num_frames = 1

        delays = []
        for sout in souts[1:]:
            if not (m := re.search(r'before, (?P<sec>\d+), (?P<nsec>\d+)', sout)):
                continue
            before = float(m['sec']) + float(m['nsec']) / 1000000000
            if not (m := re.search(r'after, (?P<sec>\d+), (?P<nsec>\d+)', sout)):
                continue
            after = float(m['sec']) + float(m['nsec']) / 1000000000
            delays.append(after - before)
        print(f'delays:')
        print(f'  min: {1000 * min(delays) / num_frames} ms')
        print(f'  max: {1000 * max(delays) / num_frames} ms')
        print(f'  avg: {1000 * sum(delays) / len(delays) / num_frames} ms')
        print(f'fps: {num_frames * len(delays) / sum(delays)}')


class Experiment(Subcommand):
    def on_parser_init(self, parser: SubcommandParser):

        parser.add_subcommands(
            Upload(),
            SquareAnim(),
            Evaluation(),
        )

    def on_command(self, args):
        pass
