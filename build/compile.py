#!/usr/bin/env python3
# PYTHON_ARGCOMPLETE_OK

import os

import sh

from yautil import SubcommandParser
from subcommands.experiment import Experiment
from subcommands.optee import Optee
from subcommands.updatesdcard import UpdateSDCard

try:
    import argcomplete
except ImportError:
    # You can use bash autocomplete with argcomplete
    # https://stackoverflow.com/questions/14597466/custom-tab-completion-in-python-argparse
    # $ pip3 install argcomplete
    # $ activate-global-python-argcomplete
    # if you are in MacOS
    # $ activate-global-python-argcomplete --dest $(brew --prefix)/etc/bash_completion.d
    pass


def __init(args):
    nice = args.nice
    if nice != 10:
        sh.sudo.renice(n=nice, p=os.getpid(), _fg=True)


if __name__ == '__main__':
    parser = SubcommandParser(argcomplete=True)
    parser.add_argument('--nice', type=int,
                        default=10,
                        help='give nice to this command',
                        shared=True)

    parser.add_subcommands(
        Optee(),
        UpdateSDCard(),
        Experiment(),
    )

    args = parser.parse_args()
    __init(args)
    parser.exec_subcommands(args)
