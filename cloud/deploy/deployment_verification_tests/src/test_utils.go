package main

import "testing"

// MyTesting expands testing.T by adding asserted checks.
type MyTesting struct {
	*testing.T
}

func (t *MyTesting) AssertEqual(expected interface{}, actual interface{}, msg string) {
	if actual != expected {
		t.Errorf("Assertion fail (%s). Expected (%v) equal to (%v)", msg, expected, actual)
	}
}

func (t *MyTesting) AssertNotEqual(expected interface{}, actual interface{}, msg string) {
	if actual == expected {
		t.Errorf("Assertion fail (%s). Expected (%v) not equal to (%v)", msg, expected, actual)
	}
}

func (t *MyTesting) AssertGreaterThan(actual int, expected int, msg string) {
	if !(actual > expected) {
		t.Errorf("Assertion fail (%s). (%d) is not greater than (%d)", msg, actual, expected)
	}
}
