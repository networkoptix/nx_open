package main

import (
	"encoding/json"
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

		jsonData, err := json.MarshalIndent(testSuiteCollectionRunner.report, "  ", "  ")
		if err != nil {
			log.Println("Failed to convert report to JSON: ", err.Error())
		} else {
			log.Println(string(jsonData))
		}

		os.Exit(2)
	}
}
