package main

import (
	"log"
	"os"
)

func main() {
	log.SetOutput(os.Stdout)

	configuration := readConfiguration()

	testSuiteCollectionRunner := NewTestSuiteCollectionRunner(&configuration)
	if err := testSuiteCollectionRunner.run(); err != nil {
		log.Println("Tests failed with error: " + err.Error())
		os.Exit(1)
	}

	if testSuiteCollectionRunner.report.totalFailureCount() > 0 {
		log.Println("Some tests failed. Report:")
		// TODO: Printing tests report.
		os.Exit(2)
	}
}
