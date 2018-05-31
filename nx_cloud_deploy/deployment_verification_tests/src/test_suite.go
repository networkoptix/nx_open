package main

type TestReport struct {
	name    string
	success bool
	// Valid only if success == false.
	err error
}

//-------------------------------------------------------------------------------------------------

type TestSuiteReport struct {
	testReports []TestReport
}

func (testSuiteReport *TestSuiteReport) failureCount() int {
	count := 0
	for _, testReport := range testSuiteReport.testReports {
		if !testReport.success {
			count++
		}
	}

	return count
}

//-------------------------------------------------------------------------------------------------

type TestsRunReport struct {
	suiteReports []TestSuiteReport
}

func (report *TestsRunReport) totalFailureCount() int {
	result := 0
	for _, suiteReport := range report.suiteReports {
		result += suiteReport.failureCount()
	}

	return result
}

func (report *TestsRunReport) success() bool {
	return report.totalFailureCount() == 0
}

//-------------------------------------------------------------------------------------------------

type TestSuite interface {
	run() TestSuiteReport
}
