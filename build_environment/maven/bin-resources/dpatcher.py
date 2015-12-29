#!/bin/python2

import os
import re
import argparse
import json
import subprocess

template_regex = re.compile('^#> (.*)$')

def get_rule_files(rules_dir):
    result = []
    for root, dir, files in os.walk(rules_dir):
        for file_name in files:
            result.append(os.path.join(root, file_name))
    return result

def get_rule_regex(file_name):
    rule_file = open(file_name)
    for i in range(1, 5):
        line = rule_file.readline()
        match = template_regex.match(line)
        if (match):
            rule_file.close()
            return match.group(1)

    rule_file.close()
    return None

def compile_rules(rules):
    result = {}
    for key, value in rules.items():
        result[key] = re.compile(value)
    return result

def get_rules(rule_files):
    result = {}
    for file_name in rule_files:
        re = get_rule_regex(file_name)
        if re:
            result[file_name] = re
    return result

def read_rules(file_name):
    rules_file = open(file_name)
    result = json.loads(rules_file.read())
    rules_file.close()
    return result

def dump_rules(file_name, rules):
    rules_file = open(file_name, 'w')
    rules_file.write(json.dumps(rules))
    rules_file.write('\n')
    rules_file.close()

def apply_patch(patch):
    command = ['patch', '-p0', '-i', patch]
    print 'Exec: ' + ' '.join(command)
    try:
        subprocess.call(command)
    except:
        return 1
    return 0

def apply_script(script):
    print 'Exec: ' + script
    try:
        subprocess.call(script)
    except:
        return 1
    return 0

def apply_rule(rule_file):
    if rule_file.endswith('.patch'):
        return apply_patch(rule_file)
    else:
        return apply_script(rule_file)

def apply_rules(artifact, rules):
    for key, value in rules.items():
        if value.match(artifact):
            print ">> Applying {0} to artifact {1}".format(key, artifact)
            if apply_rule(key) == 0:
                print ">> OK"
            else:
                print ">> Failed"

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('artifacts', type=str, help='Artifact to be patched.', nargs='*')
    parser.add_argument('--rules-dir', type=str, help='Rules directory.', default=os.path.join(os.path.dirname(os.path.realpath(__file__)), 'rules'))
    parser.add_argument('--rules-file', type=str, help='Rules description file.')
    parser.add_argument('--make-rules-file', type=str, help='Create rules description cache.')

    args = parser.parse_args()

    if args.make_rules_file:
        rules = get_rules(get_rule_files(args.rules_dir))
        dump_rules(args.make_rules_file, rules)
        return 0

    if args.rules_file:
        rules = read_rules(args.rules_file)
    else:
        rules = get_rules(get_rule_files(args.rules_dir))

    rules = compile_rules(rules)

    for artifact in args.artifacts:
        apply_rules(artifact, rules)

if __name__ == "__main__":
    main()
