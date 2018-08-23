package main

import (
	"log"
	"time"
)

type TestSuiteCollectionRunner struct {
	configuration              *Configuration
	testSuites                 []TestSuite
	testSuiteResultPipe        chan TestSuiteReport
	report                     TestsRunReport
	testSuiteCollectionFactory TestSuiteCollectionFactory
}

func NewTestSuiteCollectionRunner(configuration *Configuration) *TestSuiteCollectionRunner {
	instance := TestSuiteCollectionRunner{}
	instance.configuration = configuration
	instance.testSuiteCollectionFactory = &DefaultTestSuiteCollectionFactory{}
	return &instance
}

func (testRunner *TestSuiteCollectionRunner) run() error {
	startTime := time.Now()

	for {
		if err := testRunner.initializeTestSuites(); err != nil {
			return err
		}

		if err := testRunner.startTestSuites(); err != nil {
			return err
		}

		if err := testRunner.waitForCompletion(); err != nil {
			return err
		}

		if testRunner.report.success() {
			log.Printf("Every test suite has completed without failures")
			break
		}

		curTime := time.Now()
		if curTime.Sub(startTime) >= testRunner.configuration.maxTimeToWaitForTestsToPass {
			// There are failures in the report.
			log.Printf("Tests failed with %d failure(s)", testRunner.report.totalFailureCount())
			break
		}

		log.Printf("Tests completed with %d failure(s). Retrying after %d seconds",
			testRunner.report.totalFailureCount(), testRunner.configuration.retryTestPeriod/time.Second)

		time.Sleep(testRunner.configuration.retryTestPeriod)
	}

	return nil
}

//-------------------------------------------------------------------------------------------------

func (testRunner *TestSuiteCollectionRunner) initializeTestSuites() error {
	testRunner.testSuites = testRunner.testSuiteCollectionFactory.create(testRunner.configuration)
	testRunner.testSuiteResultPipe = make(chan TestSuiteReport, len(testRunner.testSuites))
	return nil
}

func (testRunner *TestSuiteCollectionRunner) startTestSuites() error {
	testRunner.report = TestsRunReport{}

	for _, testSuite := range testRunner.testSuites {
		go testRunner.runTestSuite(testSuite)
	}
	return nil
}

func (testRunner *TestSuiteCollectionRunner) waitForCompletion() error {
	for i := 0; i < len(testRunner.testSuites); i++ {
		testSuiteReport := <-testRunner.testSuiteResultPipe
		testRunner.report.SuiteReports = append(testRunner.report.SuiteReports, testSuiteReport)
		log.Printf("Test suite %s finished with %d failure(s)",
			testSuiteReport.Name, testSuiteReport.failureCount())
		if testSuiteReport.failureCount() > 0 {
			// TODO Interrupting other test suites that are still running
			// break
		}
	}
	return nil
}

func (testRunner *TestSuiteCollectionRunner) runTestSuite(testSuite TestSuite) {
	testSuiteRunner := TestSuiteRunner{testSuite: testSuite, configuration: testRunner.configuration}
	report := testSuiteRunner.run()
	testRunner.testSuiteResultPipe <- report
}
