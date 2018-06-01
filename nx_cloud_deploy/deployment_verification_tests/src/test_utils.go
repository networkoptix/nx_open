package main

import "testing"

// MyTesting expands testing.T by adding asserted checks.
type MyTesting struct {
	*testing.T
}

func (t *MyTesting) AssertEqual(expected int, actual int, msg string) {
	if actual != expected {
		t.Errorf("Assertion fail (%s). Expected (%d) equal to (%d)", msg, expected, actual)
	}
}

func (t *MyTesting) AssertNotEqual(expected int, actual int, msg string) {
	if actual == expected {
		t.Errorf("Assertion fail (%s). Expected (%d) not equal to (%d)", msg, expected, actual)
	}
}

func (t *MyTesting) AssertGreaterThan(actual int, expected int, msg string) {
	if !(actual > expected) {
		t.Errorf("Assertion fail (%s). (%d) is not greater than (%d)", msg, actual, expected)
	}
}
