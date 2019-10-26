#!/usr/bin/env python3

import configparser
import os
import os.path
import shlex
import sys

from subprocess import run

if __name__ == '__main__':
    submodule = False
    if len(sys.argv) == 2:
        if sys.argv[1] != 'submodule':
            print('usage: python3 pkg-standalone.py [submodule]')
            sys.exit(1)
        submodule = True

    cfg = configparser.ConfigParser()
    cfg.read('.pkg')

    os.makedirs('deps', exist_ok=True)

    for name in cfg.sections():
        url = cfg[name]['url']
        commit = cfg[name]['commit']

        path = os.path.join('deps', name)
        if os.path.exists(path):
            print('skip:', path)
            continue

        if submodule:
            cmd = 'git submodule add -f {} {}'.format(url, path)
            print(name, ' : ', cmd)
            run(shlex.split(cmd), check=True)
        else:
            cmd = 'git clone --no-checkout {} {}'.format(url, path)
            print(name, ' : ', cmd)
            run(shlex.split(cmd), check=True)

        cmd = 'git checkout {}'.format(commit)
        print(name, ' : ', cmd)
        run(shlex.split(cmd), cwd=path, check=True)

    with open('deps/CMakeLists.txt', 'w') as f:
        f.write('project(deps)\n')
        f.write('cmake_minimum_required(VERSION 3.10)\n\n')
        for name in cfg.sections():
            f.write('add_subdirectory({} EXCLUDE_FROM_ALL)\n'.format(name))
