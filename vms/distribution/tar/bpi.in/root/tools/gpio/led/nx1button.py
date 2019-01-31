#!/usr/bin/env python
import RPi.GPIO as GPIO
import os, time, sys, traceback

GPIO.setmode(GPIO.RAW)
GPIO.setup(GPIO.PH+20,GPIO.IN)
GPIO.setup(GPIO.PH+15,GPIO.OUT)
GPIO.setup(GPIO.PI+3,GPIO.OUT)
old = 1
new = 0

def switch (led_pin):
    current_value=GPIO.input(led_pin)
    if current_value == 0:
        GPIO.output(led_pin,True)
    if current_value == 1:
        GPIO.output(led_pin,False)

def reboot():
    print "Reset button released! Reboot issued"
    GPIO.output(GPIO.PH+15,True)
    GPIO.output(GPIO.PI+3,True)
    os.system('/etc/init.d/nx1boot reboot')
    sys.exit(0)

def restore():
    print "Reset button is pressed for 5 seconds! Factory Restore issued!"
    GPIO.output(GPIO.PH+15,True)
    GPIO.output(GPIO.PI+3,True)
    os.system('/etc/init.d/nx1boot restore')
    sys.exit(0)

if __name__ == "__main__":
    while True:
        new = GPIO.input(GPIO.PH+20)
        if old != new:
            if new == 0:
                GPIO.output(GPIO.PH+15,True)
                GPIO.output(GPIO.PI+3,False)
                old = new
                print "Reset button pressed!"
                for x in range(0, 25):
                    print x
                    time.sleep(0.2)
                    new = GPIO.input(GPIO.PH+20)
                    switch(GPIO.PH+15)
                    switch(GPIO.PI+3)
                    if old != new:
                        reboot()
                    else:
                        print "Reset button is still pressed!"
                new = GPIO.input(GPIO.PH+20)
                if old == new:
                    restore()
            else:
               reboot
        old = new
        time.sleep(0.5)
