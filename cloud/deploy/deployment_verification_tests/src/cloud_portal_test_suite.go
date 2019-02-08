package main

import (
	"fmt"
	"net/http"
)

const cloudPortalSuiteName string = "Portal"

type CloudPortalTestSuite struct {
	configuration *Configuration
}

func NewCloudPortalTestSuite(configuration *Configuration) *CloudPortalTestSuite {
	return &CloudPortalTestSuite{configuration}
}

func (testSuite *CloudPortalTestSuite) testServiceUp() TestSuiteReport {
	report := TestSuiteReport{Name: cloudPortalSuiteName}
	report.TestReports = append(report.TestReports, TestReport{})
	report.TestReports[0].Name = "Service up"

	url := testSuite.prepareUrl("/api/ping")

	response, err := http.Get(url)
	if err != nil {
		report.TestReports[0].Success = false
		report.TestReports[0].Err = err
		return report
	}

	if response.StatusCode != http.StatusOK {
		report.TestReports[0].Success = false
		report.TestReports[0].Err =
			fmt.Errorf("Error. Got %d status from %s", response.StatusCode, url)
		return report
	}

	report.TestReports[0].Success = true
	return report
}

func (testSuite *CloudPortalTestSuite) run() TestSuiteReport {
	report := TestSuiteReport{Name: cloudPortalSuiteName}
	return report
}

func (testSuite *CloudPortalTestSuite) prepareUrl(path string) string {
	addr := testSuite.configuration.instanceHostname
	if testSuite.configuration.httpPort != 0 {
		addr += fmt.Sprintf(":%d", testSuite.configuration.httpPort)
	}
	return "http://" + addr + path
}
