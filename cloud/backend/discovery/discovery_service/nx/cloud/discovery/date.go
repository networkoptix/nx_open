package main

import (
	"bytes"
	"errors"
	"time"
)

// Borrowed from TimeFormat in net/http package server.go file
const kHttpTimeFormat = "Mon, 02 Jan 2006 15:04:05 GMT"

//31 == len([]byte(\"Mon, 02 Jan 2006 15:04:05 GMT\"))
const kJsonBytesLen = 31

func FormatDate(t time.Time) string {
	return t.UTC().Format(kHttpTimeFormat)
}

func ParseDate(httpDate string) (time.Time, error) {
	return time.Parse(httpDate, kHttpTimeFormat)
}

type Date struct {
	Time time.Time
}

func (d *Date) MarshalJSON() ([]byte, error) {
	var buf bytes.Buffer
	buf.WriteString("\"")
	buf.WriteString(FormatDate(d.Time))
	buf.WriteString("\"")
	return buf.Bytes(), nil
}

func (d *Date) UnmarshalJSON(b []byte) error {
	if len(b) != kJsonBytesLen {
		return errors.New(
			"UnmarshalJSON requires format: \"Mon, 02 Jan 2006 15:04:05 GMT\" (with quotes)," +
				" got: " + string(b))
	}
	t, err := ParseDate(string(b[1 : len(b)-1]))
	if err == nil {
		d.Time = t
	}
	return err
}
