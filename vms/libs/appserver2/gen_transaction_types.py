import sys
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--input', type=str, help="Source transaction description file", required=True)
parser.add_argument('--output', type=str, help="Output file with unique type list", required=True)
args = parser.parse_args()

with open(args.input, 'r') as f:
    data = f.read()

dataTypes = set()
for line in data.splitlines():
    line = line.strip()
    if line.startswith('APPLY'):
        dataTypes.add(line.split(',')[2].strip())

outFile = open(args.output, 'w')
outFile.write('#define TransactionDataTypes ')
for dataType in dataTypes:
    outFile.write('({})'.format(dataType))
outFile.close()
