package main

import (
	"testing"
	"time"
)

type DummyTestSuite struct {
	broken                    bool
	runCount                  int
	revertBrokenStateAfterRun int
}

func (testSuite *DummyTestSuite) testServiceUp() TestSuiteReport {
	report := TestSuiteReport{}
	report.name = "Dummy"
	report.testReports = append(report.testReports, TestReport{"DummyServiceUp", true, nil})
	return report
}

func (testSuite *DummyTestSuite) run() TestSuiteReport {
	if testSuite.revertBrokenStateAfterRun > 0 && testSuite.runCount == testSuite.revertBrokenStateAfterRun {
		testSuite.broken = !testSuite.broken
	}
	testSuite.runCount++

	report := TestSuiteReport{}
	report.name = "Dummy"
	report.testReports = append(report.testReports, TestReport{"Dummy", !testSuite.broken, nil})
	return report
}

//-------------------------------------------------------------------------------------------------

type TestDefaultTestSuiteCollectionFactory struct {
	testSuite DummyTestSuite
}

func (factory *TestDefaultTestSuiteCollectionFactory) create(configuration *Configuration) []TestSuite {
	testSuiteColection := []TestSuite{}
	testSuiteColection = append(testSuiteColection, &factory.testSuite)
	return testSuiteColection
}

//-------------------------------------------------------------------------------------------------

type TestSuiteCollectionRunnerTestContext struct {
	t                *MyTesting
	configuration    Configuration
	runner           *TestSuiteCollectionRunner
	testSuiteFactory TestDefaultTestSuiteCollectionFactory
}

func newTestSuiteCollectionRunnerTestContext(t *testing.T) *TestSuiteCollectionRunnerTestContext {
	ctx := TestSuiteCollectionRunnerTestContext{}
	ctx.t = &MyTesting{t}
	ctx.runner = NewTestSuiteCollectionRunner(&ctx.configuration)
	ctx.runner.testSuiteCollectionFactory = &ctx.testSuiteFactory

	return &ctx
}

func (testCtx *TestSuiteCollectionRunnerTestContext) enableServiceUpTests() {
	testCtx.configuration.maxTimeToWaitForServiceUp = time.Duration(1) * time.Millisecond
}

func (testCtx *TestSuiteCollectionRunnerTestContext) givenBrokenTestSuite() {
	testCtx.testSuiteFactory.testSuite.broken = true
}

func (testCtx *TestSuiteCollectionRunnerTestContext) givenRunnerWithInfiniteRetries() {
	testCtx.configuration.maxTimeToWaitForTestsToPass = time.Duration(1) * time.Hour
	testCtx.configuration.retryTestPeriod = time.Duration(10) * time.Millisecond
}

func (testCtx *TestSuiteCollectionRunnerTestContext) givenRunnerWithNonZeroRetryTime() {
	testCtx.configuration.maxTimeToWaitForTestsToPass = time.Duration(100) * time.Millisecond
	testCtx.configuration.retryTestPeriod = time.Duration(10) * time.Millisecond
}

func (testCtx *TestSuiteCollectionRunnerTestContext) givenBrokenTestSuiteThatWillBeFixedOnRun(runNumber int) {
	testCtx.testSuiteFactory.testSuite.broken = true
	testCtx.testSuiteFactory.testSuite.revertBrokenStateAfterRun = runNumber
}

func (testCtx *TestSuiteCollectionRunnerTestContext) whenRunTests() {
	if err := testCtx.runner.run(); err != nil {
		testCtx.t.Error("Test run failed. " + err.Error())
	}
}

func (testCtx *TestSuiteCollectionRunnerTestContext) thenTestSuiteHasSucceeded() {
	testCtx.t.AssertEqual(0, testCtx.runner.report.totalFailureCount(), "totalFailureCount")
}

func (testCtx *TestSuiteCollectionRunnerTestContext) thenTestSuiteHasFailed() {
	testCtx.t.AssertNotEqual(0, testCtx.runner.report.totalFailureCount(), "totalFailureCount")
}

func (testCtx *TestSuiteCollectionRunnerTestContext) thenServiceUpTestWasInvoked() {
	testCtx.t.AssertEqual(
		"DummyServiceUp",
		testCtx.runner.report.suiteReports[0].testReports[0].name,
		"testCtx.runner.report.suiteReports[0].testReports[0].name")
}

func (testCtx *TestSuiteCollectionRunnerTestContext) andTestSuiteRunCountIs(runCount int) {
	testCtx.t.AssertEqual(runCount, testCtx.testSuiteFactory.testSuite.runCount,
		"testCtx.testSuiteFactory.testSuite.runCount")
}

func (testCtx *TestSuiteCollectionRunnerTestContext) andTestSuiteRunCountIsGreaterThan(runCount int) {
	testCtx.t.AssertGreaterThan(testCtx.testSuiteFactory.testSuite.runCount, runCount,
		"testCtx.testSuiteFactory.testSuite.runCount")
}

//-------------------------------------------------------------------------------------------------

func TestRunIsNotRetriedIfMaxWaitTimeIsZero(t *testing.T) {
	testCtx := newTestSuiteCollectionRunnerTestContext(t)

	testCtx.givenBrokenTestSuite()

	testCtx.whenRunTests()

	testCtx.thenTestSuiteHasFailed()
	testCtx.andTestSuiteRunCountIs(1)
}

func TestRunIsRetriedUntilEveryTestPasses(t *testing.T) {
	testCtx := newTestSuiteCollectionRunnerTestContext(t)

	testCtx.givenRunnerWithInfiniteRetries()
	testCtx.givenBrokenTestSuiteThatWillBeFixedOnRun(3)

	testCtx.whenRunTests()

	testCtx.thenTestSuiteHasSucceeded()
	testCtx.andTestSuiteRunCountIs(4)
}

func TestRunIsRetriedUntilMaxWaitTimePasses(t *testing.T) {
	testCtx := newTestSuiteCollectionRunnerTestContext(t)

	testCtx.givenRunnerWithNonZeroRetryTime()
	testCtx.givenBrokenTestSuite()

	testCtx.whenRunTests()

	testCtx.thenTestSuiteHasFailed()
	testCtx.andTestSuiteRunCountIsGreaterThan(0)
}

func TestServiceUpIsInvoked(t *testing.T) {
	testCtx := newTestSuiteCollectionRunnerTestContext(t)
	testCtx.enableServiceUpTests()

	testCtx.whenRunTests()
	testCtx.thenServiceUpTestWasInvoked()
}

func TestSuiteIsPerformedAfterServiceUp(t *testing.T) {
	// TODO
}

func TestServiceUpIsRetriedUntilPasses(t *testing.T) {
	// TODO
}

func TestServiceUpIsRetriedUntilTimeoutPasses(t *testing.T) {
	// TODO
}
