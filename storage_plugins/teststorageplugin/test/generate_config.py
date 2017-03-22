#!/usr/bin/env python3

import argparse
import os
import sys
import json


def parseArgs():
    parser = argparse.ArgumentParser()
    parser.add_argument('--chunk_count', type=int, default=10000,
                        help='chunks per camera count')
    parser.add_argument('--camera_count', type=int, default = 10,
                        help='total cameras count')
    parser.add_argument('--chunk_duration', type=int, default=60000,
                        help='chunk duration')
    parser.add_argument('--sample', help='path to the sample file', required=True)
    parser.add_argument('--start_time', type=int, default=1427019399000,
                        help='start time point')
    return parser.parse_args()


def main():
    args = parseArgs()
    try:
        os.path.isfile(args.sample)
    except:
        print('sample file {} not found'.format(args.sample))
        sys.exit(-1)
    result = {'sample': os.path.abspath(args.sample), 'cameras': []}
    for camId in range(args.camera_count):
        currentStartTime = args.start_time
        cameraObj = {'id': 'camera' + str(camId), 'hi': [], 'low': []}
        for chunkId in range(args.chunk_count):
            cameraObj['hi'].append({'durationMs': str(args.chunk_duration), 
                                    'startTimeMs': str(currentStartTime) })
            currentStartTime += args.chunk_duration + 1
        result['cameras'].append(cameraObj)
    print(json.dumps(result))


if __name__ == '__main__':
    main()