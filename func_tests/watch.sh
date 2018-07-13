#!/bin/bash

watch -n.1 'ls -l ~/.func_tests/*.lock /tmp/func_tests/*.lock ; tail -n+1 ~/.func_tests/*registry*.yaml /tmp/func_tests/*registry*.yaml'