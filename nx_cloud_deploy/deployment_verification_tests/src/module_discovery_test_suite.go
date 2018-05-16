package main

import (
	"encoding/xml"
	"fmt"
	"io/ioutil"
	"net"
	"net/http"
	"net/url"
)

var moduleNamesToVerify = []string{"hpm", "cdb"}

type CloudModules struct {
	Modules []struct {
		Name string `xml:"resName,attr"`
		Url  string `xml:"resValue,attr"`
	} `xml:"set"`
}

func (cloudModules *CloudModules) getModuleUrl(name string) (*url.URL, error) {
	for _, module := range cloudModules.Modules {
		if module.Name == name {
			mediatorURL, err := url.Parse(module.Url)
			if err != nil {
				return nil, err
			}

			return mediatorURL, nil
		}
	}

	return nil, fmt.Errorf("Module %s not found in cloud_modules.xml", name)
}

//-------------------------------------------------------------------------------------------------

type ModuleDiscoveryTestSuite struct {
	configuration *Configuration
}

func NewModuleDiscoveryTestSuite(configuration *Configuration) *ModuleDiscoveryTestSuite {
	return &ModuleDiscoveryTestSuite{configuration}
}

func (testSuite *ModuleDiscoveryTestSuite) run() TestSuiteReport {
	testSuiteReport := TestSuiteReport{}

	testSuiteReport.testReports = append(testSuiteReport.testReports, testSuite.verifyEveryModuleIsAccesible())

	return testSuiteReport
}

func (testSuite *ModuleDiscoveryTestSuite) verifyEveryModuleIsAccesible() TestReport {
	cloudModules, err := testSuite.fetchCloudModulesXml()
	if err != nil {
		return TestReport{"verifyEveryModuleIsAccesible", false, err}
	}

	for _, moduleName := range moduleNamesToVerify {
		moduleUrl, err := cloudModules.getModuleUrl(moduleName)
		if err != nil {
			return TestReport{"verifyEveryModuleIsAccesible", false, err}
		}

		if err := testSuite.verifyUrlPointsToAValidResource(moduleUrl); err != nil {
			return TestReport{"verifyEveryModuleIsAccesible", false, err}
		}
	}

	return TestReport{"verifyEveryModuleIsAccesible", true, nil}
}

func (testSuite *ModuleDiscoveryTestSuite) fetchCloudModulesXml() (CloudModules, error) {
	addr := testSuite.configuration.instanceHostname
	if testSuite.configuration.httpPort != 0 {
		addr += fmt.Sprintf(":%d", testSuite.configuration.httpPort)
	}
	url := "http://" + addr + "/api/cloud_modules.xml"

	response, err := http.Get(url)
	if err != nil {
		return CloudModules{}, err
	}

	if response.StatusCode != http.StatusOK {
		return CloudModules{}, fmt.Errorf("Error. Got %d status from %s", response.StatusCode, url)
	}

	cloudModulesXml, err := ioutil.ReadAll(response.Body)
	if err != nil {
		return CloudModules{}, err
	}

	var cloudModules CloudModules
	if err := xml.Unmarshal(cloudModulesXml, &cloudModules); err != nil {
		return CloudModules{}, err
	}

	return cloudModules, nil
}

func (testSuite *ModuleDiscoveryTestSuite) verifyUrlPointsToAValidResource(resourceUrl *url.URL) error {
	if resourceUrl.Scheme == "http" {
		return testSuite.verifyHttpUrlPointsToAValidResource(resourceUrl)
	} else {
		return testSuite.verifyGenericUrlPointsToAValidResource(resourceUrl)
	}
}

func (testSuite *ModuleDiscoveryTestSuite) verifyHttpUrlPointsToAValidResource(resourceUrl *url.URL) error {
	_, err := http.Get(resourceUrl.String())
	return err
}

func (testSuite *ModuleDiscoveryTestSuite) verifyGenericUrlPointsToAValidResource(resourceUrl *url.URL) error {
	_, err := net.Dial("tcp", fmt.Sprintf("%s:%s", resourceUrl.Hostname(), resourceUrl.Port()))
	return err
}
