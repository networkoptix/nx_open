package main

type TestSuiteCollectionRunner struct {
	configuration       Configuration
	testSuites          []TestSuite
	testSuiteResultPipe chan TestSuiteReport
	report              TestsRunReport
}

func NewTestSuiteCollectionRunner(configuration Configuration) *TestSuiteCollectionRunner {
	instance := TestSuiteCollectionRunner{}
	instance.configuration = configuration
	return &instance
}

func (testRunner *TestSuiteCollectionRunner) run() error {
	if err := testRunner.initializeTestSuites(); err != nil {
		return err
	}

	if err := testRunner.startTestSuites(); err != nil {
		return err
	}

	if err := testRunner.waitForCompletion(); err != nil {
		return err
	}

	return nil
}

//-------------------------------------------------------------------------------------------------

func (testRunner *TestSuiteCollectionRunner) initializeTestSuites() error {
	testRunner.testSuites = append(testRunner.testSuites, NewCloudPortalTestSuite(testRunner.configuration))
	testRunner.testSuites = append(testRunner.testSuites, NewModuleDiscoveryTestSuite(testRunner.configuration))
	testRunner.testSuiteResultPipe = make(chan TestSuiteReport, len(testRunner.testSuites))
	return nil
}

func (testRunner *TestSuiteCollectionRunner) startTestSuites() error {
	for _, testSuite := range testRunner.testSuites {
		go testRunner.runTestSuite(testSuite)
	}
	return nil
}

func (testRunner *TestSuiteCollectionRunner) waitForCompletion() error {
	for i := 0; i < len(testRunner.testSuites); i++ {
		testSuiteReport := <-testRunner.testSuiteResultPipe
		testRunner.report.suiteReports = append(testRunner.report.suiteReports, testSuiteReport)
		if testSuiteReport.failureCount() > 0 {
			// TODO Interrupting other test suites that are still running
			// break
		}
	}
	return nil
}

func (testRunner *TestSuiteCollectionRunner) runTestSuite(testSuite TestSuite) {
	report := testSuite.run()
	testRunner.testSuiteResultPipe <- report
}
