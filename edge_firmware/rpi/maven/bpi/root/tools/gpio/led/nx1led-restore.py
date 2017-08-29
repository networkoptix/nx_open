#!/usr/bin/env python
# VERSION: @release.version@.@buildNumber@
import RPi.GPIO as GPIO
import time

GPIO.setmode(GPIO.RAW)
GPIO.setup(GPIO.PH+15,GPIO.OUT)
GPIO.setup(GPIO.PI+3,GPIO.OUT)

while True:

	GPIO.output(GPIO.PH+15,True)
	GPIO.output(GPIO.PI+3,False)
	time.sleep(0.2)

	GPIO.output(GPIO.PH+15,False)
	GPIO.output(GPIO.PI+3,True)
	time.sleep(0.2)

