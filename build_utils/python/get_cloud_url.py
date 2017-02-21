#!/usr/bin/env python
"""
Writes cloud url to target file (or std output if not given).
Cloud url is automatically received from cloud host finder.
"""

import os
import sys
import argparse
import requests

verbose = False

def printError(error):
    print >> sys.stderr, "ERROR: {0}".format(error)

def getHostOnline(instance, customization, timeout):
    url = 'https://ireg.hdw.mx/api/v1/cloudhostfinder/?group={0}&vms_customization={1}'.format(instance, customization)
    global verbose
    if verbose:
        print url
    
    try:
        res = requests.get(url, timeout = timeout)
        result=res.text
    except requests.exceptions.RequestException as e:
        printError(e)
        return None
        
    failed = ' ' is result
    if failed:
        printError(result)
        return None
    return result


def writeToTarget(host, target):
    if not host:
        return
    if not target:
        print host
    else:
        with open(target, 'w') as f:
            print >> f, "cloudHost={0}".format(host)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-v', '--verbose', action='store_true', help="verbose output")
    parser.add_argument('-i', '--instance', metavar="instance", default="", help="cloud instance name (default is empty)", required = True)
    parser.add_argument('-c', '--customization', help="customization name", required = True)    
    parser.add_argument('-t', '--target', help="output to file")
    parser.add_argument('--host', help="directly provided host")
    parser.add_argument('--timeout', default=10, type=int, help="request timeout")

    args = parser.parse_args()

    print "Requesting cloud host for customization {0}, instance {1}".format(args.customization, args.instance)
    validHost = args.host and not args.host.startswith('$')
    if validHost:
        print "Provided host {0}, network request will not occur".format(args.host)

    global verbose
    verbose = args.verbose
    if verbose and args.target:
        print "Result will be saved to {0}".format(args.target)
        
    host = args.host if validHost else getHostOnline(args.instance, args.customization, args.timeout)
    if host:
        writeToTarget(host, args.target)
    else:
        sys.exit(1)


if __name__ == '__main__':
    main()
