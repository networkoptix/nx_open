package main

type TestReport struct {
	Name    string
	Success bool
	// Valid only if success == false.
	Err error
}

//-------------------------------------------------------------------------------------------------

type TestSuiteReport struct {
	Name        string
	TestReports []TestReport
}

func (testSuiteReport *TestSuiteReport) failureCount() int {
	count := 0
	for _, testReport := range testSuiteReport.TestReports {
		if !testReport.Success {
			count++
		}
	}

	return count
}

//-------------------------------------------------------------------------------------------------

type TestsRunReport struct {
	SuiteReports []TestSuiteReport
}

func (report *TestsRunReport) totalFailureCount() int {
	result := 0
	for _, suiteReport := range report.SuiteReports {
		result += suiteReport.failureCount()
	}

	return result
}

func (report *TestsRunReport) success() bool {
	return report.totalFailureCount() == 0
}

//-------------------------------------------------------------------------------------------------

type TestSuite interface {
	// Tests state of services this suite tests depend on.
	testServiceUp() TestSuiteReport
	run() TestSuiteReport
}
