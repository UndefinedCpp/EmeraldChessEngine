import argparse
import os
from datetime import datetime
import subprocess

# Make sure you have fastchess installed
if os.path.exists('./fastchess.exe') == False:
    raise RuntimeError('fastchess required')
# And opening books exist
if os.path.exists('./openingbook') == False:
    raise RuntimeError('opening books required')

OPENING_BOOK = './openingbook/8moves_v3.pgn'
OPENING_BOOK_FORMAT = 'pgn'
OUTPUT_FILENAME = f'enginetest_{datetime.now().strftime("%H_%M_%S")}.pgn'


parser = argparse.ArgumentParser()
parser.add_argument('engine_1', type=str)
parser.add_argument('engine_2', type=str)
parser.add_argument('--ref-engine', type=str, default=None)
parser.add_argument('--ref-engine2', type=str, default=None)
parser.add_argument('--games', '-n', type=int, default=10)
parser.add_argument('--timecontrol', '-t', type=str, default='10+0.1')
parser.add_argument('--clear', '-c', action='store_true')
args = parser.parse_args()

if args.clear:
    os.remove('./debug.log')
    os.remove('./config.json')
    files = [s for s in os.listdir('.') if (s.startswith('enginetest_'))]
    for f in files:
        os.remove(f)
    exit(0)

engine_1_name = args.engine_1
engine_2_name = args.engine_2
ref_engine = args.ref_engine
ref_engine2 = args.ref_engine2
if engine_1_name == engine_2_name:
    engine_1_name = f'{engine_1_name}_1'
    engine_2_name = f'{engine_2_name}_2'

if ref_engine is not None:
    ref_engine_name = ref_engine
    ref_engine_call = ['-engine', 'cmd=./build/' +
                       ref_engine+'.exe', 'name='+ref_engine_name]
else:
    ref_engine_call = []

if ref_engine2 is not None:
    ref_engine2_name = ref_engine2
    ref_engine2_call = ['-engine', 'cmd=./build/' +
                       ref_engine2+'.exe', 'name='+ref_engine2_name]
else:
    ref_engine2_call = []

command_line = [
    './fastchess',
    '-event',   # PGN event head
    'test',
    '-games',   # Number of games to play
    str(args.games),
    '-variant',  # Play standard chess
    'standard',
    '-recover',  # Recover engine in case of crash
    '-draw',  # Early draw adjundication:
    'movenumber=40',  # - after 40 moves,
    'movecount=3',  # - for 3 consecutive moves,
    'score=20',  # - below 20cp evaluation
    '-maxmoves',  # draw after 150 moves
    '150',
    '-openings',  # specify opening book
    'file='+OPENING_BOOK,
    'format='+OPENING_BOOK_FORMAT,
    'order=random',
    '-output',  # Output format settings
    'format=fastchess',
    '-pgnout',
    'file='+OUTPUT_FILENAME,
    '-autosaveinterval',
    '1',
    '-engine',  # Set up engine 1
    'cmd=./build/'+args.engine_1+'.exe',
    'name='+engine_1_name,
    '-engine',  # Set up engine 2
    'cmd=./build/'+args.engine_2+'.exe',
    'name='+engine_2_name,
    *ref_engine_call,
    *ref_engine2_call,
    '-each',
    'tc='+args.timecontrol,
    '-log',
    'file=debug.log',
    'level=info',
    '-concurrency',
    '4'
]

ret = subprocess.run(command_line)
print('return code:', ret.returncode)
