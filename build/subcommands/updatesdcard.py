import argparse
import getpass
import os
import platform
import sys
import time

import sh

from yautil import Subcommand

root_dir: str = os.path.realpath(os.path.join(__file__, '..', '..', '..'))


def get_mysudo():
    prompt='[sudo] password for ' + getpass.getuser() + ': '
    while True:
        my_sudo = sh.sudo.bake(S=True, _in=getpass.getpass(prompt=prompt)+'\n')
        try:
            my_sudo.echo(n=True)
        except sh.ErrorReturnCode:
            print('Sorry, try again.')
            continue
        return my_sudo


def get_cmd_args(cmd):
    args = [cmd._path]
    if cmd._partial:
        args.extend(cmd._partial_baked_args)
    return [e.decode('utf-8') for e in args]


class UpdateSDCard(Subcommand):
    def on_parser_init(self, parser: argparse.ArgumentParser):
        if platform.system() == 'Darwin':
            def_mount = 'fuse-ext2'
            def_mount_option = ','.join(['rw+', 'uid=' + str(os.getuid()), 'gid=' + str(os.getgid())])
        elif platform.system() == 'Linux':
            def_mount = 'mount'
            def_mount_option = ','.join(['rw', 'user=' + str(getpass.getuser())])
        else:
            print('error: only OSX and Linux are supported')
            quit()

        parser.add_argument('device', type=str,
                            help='device path. e.g., /dev/sdb or /dev/disk2')
        parser.add_argument('-o', '--optee-out', type=str,
                            default=os.path.join(root_dir, 'out'),
                            help='directory path.')
        parser.add_argument('--mount', type=str,
                            default=def_mount,
                            help='ext4 volume mount command. default: ' + def_mount)
        parser.add_argument('--mount-option', type=str,
                            default=def_mount_option,
                            help='mount command option to be writable. default: ' + def_mount_option)
        parser.add_argument('-i', type=str,
                            default=os.path.join(os.environ['HOME'], '.ssh', 'id_rsa'),
                            help='ssh private key path (default: ~/.ssh/id_rsa)')

    def on_command(self, args):
        try:
            mount = sh.Command(args.mount)
        except sh.CommandNotFound:
            print('error: command ' + args.mount + ' not found. please specify with --mount')
            quit()

        def_ssh_config = os.path.join(os.environ['HOME'], '.ssh', 'config')
        ssh_opt = '-i {:s}'.format(os.path.join(os.environ['HOME'], '.ssh', 'id_rsa'))
        if os.path.isfile(def_ssh_config):
            ssh_opt += ' -F {:s}'.format(def_ssh_config)

        rsync = sh.rsync.bake(r=True, p=True, t=True, z=True, L=True, P=True, c=True,
                              # https://unix.stackexchange.com/a/175673
                              e='ssh ' + ssh_opt)

        if platform.system() == 'Darwin':
            # disk1s1 = disk1 + s + 1
            partition_prefix = 's'
        elif platform.system() == 'Linux':
            # sda1 = sda + 1
            partition_prefix = ''

        mysudo = get_mysudo()

        for partition_no in ['1']:
            tmp_dir='.__tmp' + partition_no
            device_partition = args.device + partition_prefix + partition_no
            out_subdir = os.path.join(args.optee_out, partition_no)

            if not os.path.exists(device_partition):
                print('error: ' + device_partition + ' does not exist')
                continue

            os.makedirs(tmp_dir, exist_ok=True)
            mysudo(args.mount, device_partition, tmp_dir, o=args.mount_option)
            # mysudo('chown', getpass.getuser() + ':' + getpass.getuser(), '-R', tmp_dir)
            try:
                # rsync(out_subdir + '/', tmp_dir, _out=sys.stdout)
                mysudo(*get_cmd_args(rsync), *[out_subdir + '/', tmp_dir], _out=sys.stdout)
            except sh.ErrorReturnCode as err:
                print(err)

            # print(sh.ls('-al', tmp_dir))
            mysudo('umount', tmp_dir)
            sh.rmdir(tmp_dir)

