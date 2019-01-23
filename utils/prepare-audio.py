#!/usr/local/bin/python2.7

import sys
import os
import shutil
import subprocess
import getopt
import argparse
import csv
import json
import re
import pprint

pp = pprint.PrettyPrinter(indent=4)


def walkDirectoryForInput(inputFiles, pattern, path):
    fileNames = os.listdir(path)
    for fileName in fileNames:
        fullFileName = os.path.join(path, fileName)

        if os.path.isdir(fullFileName):
            walkDirectoryForInput(inputFiles, pattern, fullFileName)
        elif re.match(pattern, fullFileName):
            inputFiles.append(fullFileName)

def main(args):
    if not os.path.exists(args.input) or not os.path.isdir(args.input):
        return
    if not os.path.exists(args.output):
        os.mkdir(args.output)

    inputFiles = []
    walkDirectoryForInput(inputFiles, '^.*\.wav$', args.input)

    with open(os.path.join(args.output, 'audio.csv'), 'wb') as csv_file:
        fieldnames = ['name', 'index']
        writer = csv.DictWriter(csv_file, delimiter=',', fieldnames=fieldnames)
        writer.writeheader()
        for i, inputFile in enumerate(inputFiles):
            name = os.path.relpath(inputFile, args.input)

            ext = os.path.splitext(inputFile)[1]
            idx = '%04d' % i

            writer.writerow({
                'name': name,
                'index': idx + ext
            })

            shutil.copyfile(inputFile, os.path.join(args.output, idx + ext))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate audio files for VHS''s NFC-enabled door')
    parser.add_argument('-i', action="store", dest="input", help="path of source audio files")
    parser.add_argument('-o', action="store", dest="output", help="path to output processed audio files")

    main(parser.parse_args())
