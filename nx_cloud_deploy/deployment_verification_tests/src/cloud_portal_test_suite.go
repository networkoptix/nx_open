package main

type CloudPortalTestSuite struct {
	configuration *Configuration
}

func NewCloudPortalTestSuite(configuration *Configuration) *CloudPortalTestSuite {
	return &CloudPortalTestSuite{configuration}
}

func (testSuite *CloudPortalTestSuite) run() TestSuiteReport {
	return TestSuiteReport{}
}
