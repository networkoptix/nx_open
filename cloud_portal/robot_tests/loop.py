from os import system
import time
import datetime
import language_test

hour = 20
minute = 0
checkInterval = 1800
runTime = datetime.datetime.now().replace(hour=hour, minute=minute)

while True:
	if datetime.datetime.now() >= runTime:
		language_test.allLanguages()
		runTime += datetime.timedelta(days=1)
	time.sleep(checkInterval)