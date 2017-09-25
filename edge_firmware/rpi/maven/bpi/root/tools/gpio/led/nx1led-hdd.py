#!/usr/bin/env python
# VERSION: @release.version@.@buildNumber@
import RPi.GPIO as GPIO
import time

GPIO.setmode(GPIO.RAW)
GPIO.setup(GPIO.PH+15,GPIO.OUT)
GPIO.setup(GPIO.PI+3,GPIO.OUT)

GPIO.output(GPIO.PH+15,False)
GPIO.output(GPIO.PI+3,True)

