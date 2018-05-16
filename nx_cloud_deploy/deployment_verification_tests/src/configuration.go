package main

import "flag"

// Configuration for the test suite.
type Configuration struct {
	instanceHostname string
	httpPort         int
}

func readConfiguration() Configuration {
	cloudHost := flag.String("cloud-host", "nxvms.com", "a string")
	flag.Parse()

	configuration := Configuration{*cloudHost, 80}

	return configuration
}
