package main

import (
	"fmt"
	"net"
	"net/http"
	"strconv"
	"strings"
	"testing"
	"time"
)

const cloudModulesXmlWithInvalidMediatorUrl string = `<?xml version="1.0" encoding="utf-8"?>
<sequence>
	<set resName="cdb" resValue="http://127.0.0.1:%port%" />
    <set resName="hpm" resValue="stun://None:3345" />
    <set resName="notification_module" resValue="http://127.0.0.1:%port%" />
</sequence>`

const cloudModulesXmlWithInvalidCdbUrl string = `<?xml version="1.0" encoding="utf-8"?>
<sequence>
	<set resName="cdb" resValue="https://
	nxvms.com:443" />
    <set resName="hpm" resValue="stun://127.0.0.1:%port%" />
    <set resName="notification_module" resValue="http://127.0.0.1:%port%" />
</sequence>`

const validCloudModulesXmlTemplate string = `<?xml version="1.0" encoding="utf-8"?>
<sequence>
    <set resName="cdb" resValue="http://127.0.0.1:%port%" />
    <set resName="hpm" resValue="stun://127.0.0.1:%port%" />
    <set resName="notification_module" resValue="http://127.0.0.1:%port%" />
</sequence>`

type ModuleDiscoveryTestContext struct {
	cloudModuleXmlServer           *http.Server
	cloudModuleXmlServerPort       int
	cloudModuleEmulationServer     *http.Server
	cloudModuleEmulationServerPort int
	t                              *testing.T
	report                         TestSuiteReport
}

func NewModuleDiscoveryTestContext(t *testing.T) *ModuleDiscoveryTestContext {
	moduleDiscoveryTestContext := ModuleDiscoveryTestContext{}
	moduleDiscoveryTestContext.t = t
	return &moduleDiscoveryTestContext
}

func (testCtx *ModuleDiscoveryTestContext) givenCloudModulesXmlServerWithContents(contents string) {
	httpServer := http.Server{}
	httpServer.Addr = "127.0.0.1:0"

	serverMux := http.NewServeMux()
	serverMux.HandleFunc(
		"/api/cloud_modules.xml",
		func(w http.ResponseWriter, r *http.Request) {
			fmt.Fprintf(w, contents)
		})
	httpServer.Handler = serverMux

	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		testCtx.t.Fail()
	}

	go httpServer.Serve(listener)

	testCtx.cloudModuleXmlServer = &httpServer
	testCtx.cloudModuleXmlServerPort = listener.Addr().(*net.TCPAddr).Port
}

func (testCtx *ModuleDiscoveryTestContext) givenValidCloudModulesXmlServer() {
	testCtx.givenCloudModulesXmlServer(validCloudModulesXmlTemplate)
}

func (testCtx *ModuleDiscoveryTestContext) givenCloudModulesXmlServer(cloudModulesXmlTemplate string) {
	testCtx.givenCloudModulesXmlServerWithContents("dummy response")
	testCtx.cloudModuleEmulationServer = testCtx.cloudModuleXmlServer
	testCtx.cloudModuleEmulationServerPort = testCtx.cloudModuleXmlServerPort

	cloudModulesXml := strings.Replace(cloudModulesXmlTemplate, "%port%", strconv.Itoa(testCtx.cloudModuleEmulationServerPort), -1)
	testCtx.givenCloudModulesXmlServerWithContents(cloudModulesXml)
}

func (testCtx *ModuleDiscoveryTestContext) whenVerifyCloudModuleXml() {
	conf := NewConfiguration()
	conf.instanceHostname = "127.0.0.1"
	conf.httpPort = testCtx.cloudModuleXmlServerPort
	conf.maxTimeToWaitForTestsToPass = time.Duration(0)

	moduleDiscoveryTestSuite := NewModuleDiscoveryTestSuite(&conf)
	testCtx.report = moduleDiscoveryTestSuite.run()
}

func (testCtx *ModuleDiscoveryTestContext) thenVerificationHasFailed() {
	if testCtx.report.failureCount() == 0 {
		testCtx.t.Fail()
	}
}

func (testCtx *ModuleDiscoveryTestContext) thenVerificationHasSucceeded() {
	if testCtx.report.failureCount() > 0 {
		testCtx.t.Fail()
	}
}

func assertIncorrectModuleUrlIsDetected(t *testing.T, cloudModulesXml string) {
	testCtx := NewModuleDiscoveryTestContext(t)

	testCtx.givenCloudModulesXmlServer(cloudModulesXml)
	defer testCtx.cloudModuleXmlServer.Close()

	testCtx.whenVerifyCloudModuleXml()
	testCtx.thenVerificationHasFailed()
}

//-------------------------------------------------------------------------------------------------

func TestModuleDiscoveryIncorrectModuleUrlIsDetected(t *testing.T) {
	assertIncorrectModuleUrlIsDetected(t, cloudModulesXmlWithInvalidCdbUrl)
	assertIncorrectModuleUrlIsDetected(t, cloudModulesXmlWithInvalidMediatorUrl)
}

func TestModuleDiscoveryCorrectModuleUrlIsDetected(t *testing.T) {
	testCtx := NewModuleDiscoveryTestContext(t)

	testCtx.givenValidCloudModulesXmlServer()
	defer testCtx.cloudModuleEmulationServer.Close()
	defer testCtx.cloudModuleXmlServer.Close()

	testCtx.whenVerifyCloudModuleXml()
	testCtx.thenVerificationHasSucceeded()
}
