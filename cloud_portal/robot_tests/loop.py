from os import system
import time
import datetime
runTime = datetime.datetime.now()
runTime = runTime.replace(hour=20, minute=0)
while True:
	if datetime.datetime.now() >= runTime:
		system('python language-test.py')
		runTime = runTime + datetime.timedelta(day=1)
	time.sleep(1800)