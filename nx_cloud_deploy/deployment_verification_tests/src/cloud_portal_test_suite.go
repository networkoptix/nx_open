package main

type CloudPortalTestSuite struct {
	configuration *Configuration
}

func NewCloudPortalTestSuite(configuration *Configuration) *CloudPortalTestSuite {
	return &CloudPortalTestSuite{configuration}
}

func (testSuite *CloudPortalTestSuite) testServiceUp() TestSuiteReport {
	// TODO
	return TestSuiteReport{}
}

func (testSuite *CloudPortalTestSuite) run() TestSuiteReport {
	// TODO
	return TestSuiteReport{}
}
