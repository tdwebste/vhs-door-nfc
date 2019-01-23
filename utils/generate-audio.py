#!/usr/local/bin/python2.7

import sys
import os
import subprocess
import getopt
import argparse
import csv
import json
import pprint

pp = pprint.PrettyPrinter(indent=4)


def sanitizeName(origName):
    return origName.replace(' ', '-');


def main(args):
    if not os.path.exists(args.output):
        os.mkdir(args.output)

    lines = []
    with open(args.lines) as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            row['Name'] = sanitizeName(row['Name'])
            lines.append(row)

    for line in lines:
        outputFile = os.path.join(args.output, line['Name'] + ".wav")

        cmd = 'say -o %s --data-format=LEF32@22050 -v Fiona -r 250 "%s"' % (outputFile, line['Line'])
        pp.pprint(cmd)
        subprocess.call(cmd, shell=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate audio files for VHS''s NFC-enabled door')
    parser.add_argument('-l', action="store", dest="lines", help="path to csv of input lines to generate")
    parser.add_argument('-o', action="store", dest="output", help="path to output generated audio")

    main(parser.parse_args())
