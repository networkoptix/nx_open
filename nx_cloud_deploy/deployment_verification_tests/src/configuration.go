package main

import (
	"flag"
	"time"
)

// Configuration for the test suite.
type Configuration struct {
	instanceHostname string
	// Override cloud's default HTTP port. For testing purpose only.
	httpPort int
	// If non-zero, then test is repeated until it passes or time duration specified here passes.
	maxTimeToWaitForTestsToPass time.Duration
	retryTestPeriod             time.Duration
}

const defaultInstanceHostname string = "nxvms.com"
const defaultHttpPort int = 80
const defaultMaxTimeToWaitForTestsToPass time.Duration = time.Duration(0) * time.Second
const defaultRetryTestPeriod time.Duration = time.Duration(10) * time.Second

func NewConfiguration() Configuration {
	configuration := Configuration{}
	configuration.instanceHostname = defaultInstanceHostname
	configuration.httpPort = defaultHttpPort
	configuration.maxTimeToWaitForTestsToPass = defaultMaxTimeToWaitForTestsToPass
	configuration.retryTestPeriod = defaultRetryTestPeriod
	return configuration
}

func readConfiguration() Configuration {
	cloudHost := flag.String("cloud-host", defaultInstanceHostname, "")

	maxTimeToWaitForTestsToPass := flag.Duration(
		"max-time-to-wait",
		defaultMaxTimeToWaitForTestsToPass,
		`Maximum duration tests are retried in the hope of passing. 
Default 0 (no timeout). 
E.g., "300ms", "2h45m". Valid time units are "ns", "us", "ms", "s", "m", "h"`)

	flag.Parse()

	configuration := NewConfiguration()
	configuration.instanceHostname = *cloudHost
	configuration.maxTimeToWaitForTestsToPass = *maxTimeToWaitForTestsToPass

	return configuration
}
