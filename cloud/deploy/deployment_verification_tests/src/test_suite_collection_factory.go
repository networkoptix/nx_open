package main

type TestSuiteCollectionFactory interface {
	create(configuration *Configuration) []TestSuite
}

//-------------------------------------------------------------------------------------------------

type DefaultTestSuiteCollectionFactory struct {
}

func (factory *DefaultTestSuiteCollectionFactory) create(configuration *Configuration) []TestSuite {
	testSuiteColection := []TestSuite{}
	testSuiteColection = append(testSuiteColection, NewCloudPortalTestSuite(configuration))
	testSuiteColection = append(testSuiteColection, NewModuleDiscoveryTestSuite(configuration))
	return testSuiteColection
}
