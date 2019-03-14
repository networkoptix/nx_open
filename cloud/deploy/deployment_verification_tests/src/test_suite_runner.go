package main

import (
	"log"
	"time"
)

type TestSuiteRunner struct {
	testSuite     TestSuite
	configuration *Configuration
}

const delayBetweenServiceUpCheckRetries time.Duration = time.Second

func (runner *TestSuiteRunner) run() TestSuiteReport {
	report := TestSuiteReport{}
	if runner.configuration.maxTimeToWaitForServiceUp > 0 {
		report = runner.waitForServiceUp()
		if report.failureCount() > 0 {
			log.Printf("Test suite %s dependent services never got up", report.Name)
			return report
		}
	}

	testReport := runner.testSuite.run()
	testReport.TestReports = append(report.TestReports, testReport.TestReports...)

	log.Printf("%s test suite has completed with %d failure(s)",
		testReport.Name, testReport.failureCount())
	return testReport
}

func (runner *TestSuiteRunner) waitForServiceUp() TestSuiteReport {
	startTime := time.Now()
	for {
		report := runner.testSuite.testServiceUp()

		if report.failureCount() == 0 {
			return report
		}

		curTime := time.Now()
		if curTime.Sub(startTime) >= runner.configuration.maxTimeToWaitForServiceUp {
			return report
		}

		time.Sleep(delayBetweenServiceUpCheckRetries)
	}
}
