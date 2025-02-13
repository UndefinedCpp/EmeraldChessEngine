import os
import argparse
import subprocess

parser = argparse.ArgumentParser(description='Build the project')
parser.add_argument('--optimize', '-O', action='store_true',
                    help='Optimize the build', default=True)
parser.add_argument('--arch', '-a', type=str, default='native')
parser.add_argument('--target', type=str, default='engine.exe')
parser.add_argument('--run', '-r', action='store_true',
                    help='Run the program after building', default=False)
parser.add_argument('--static', action='store_true')
parser.add_argument('--iterative', action='store_true')

SOURCE_DIR = './src'

if __name__ == '__main__':
    args = parser.parse_args()

    source_files = []
    for filename in os.listdir(SOURCE_DIR):
        if filename.endswith('.c') or filename.endswith('.cpp'):
            source_files.append(os.path.join(SOURCE_DIR, filename))

    if args.iterative:
        if os.path.exists('./build/engine.old.exe'):
            os.remove('./build/engine.old.exe')
        if os.path.exists('./build/engine.exe'):
            os.rename('./build/engine.exe', './build/engine.old.exe')

    command = [
        'g++',
        '-O3' if args.optimize else '',
        *source_files,
        '-Wall',
        '-std=c++17',
        f'-march={args.arch}',
        '-o',
        f'./build/{args.target}',
    ]
    if args.static:
        command.extend([
            '-static-libstdc++',
            '-static-libgcc',
            '-static'
        ])

    print(f'Executing {" ".join(command)}')
    ret = subprocess.run(command).returncode
    print('Build finished with code {}'.format(ret))

    if args.run and (ret == 0):
        print('== Engine', '=' * 70)
        ret = subprocess.run(['./build/engine.exe']).returncode
        print('=' * 80)
        print('Program finished with code {}'.format(ret))
