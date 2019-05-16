package main

import (
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"
)

func nodeLifetime() time.Duration {
	const defaultLifetime = time.Minute * time.Duration(5)

	lifetimeStr := os.Getenv("NODE_LIFETIME")
	if len(lifetimeStr) == 0 {
		log.Println("Missing environment variable NODE_LIFETIME, using %s", defaultLifetime)
		return defaultLifetime
	}

	duration := time.Minute
	if strings.HasSuffix(lifetimeStr, "m") {
		duration = time.Minute
	} else if strings.HasSuffix(lifetimeStr, "s") {
		duration = time.Second
	}

	lifetime, err := strconv.Atoi(lifetimeStr[:len(lifetimeStr)-1])
	if err != nil {
		log.Println("Error parsing NODE_LIFETIME environment variable: ", err)
		return defaultLifetime
	}
	return duration * time.Duration(lifetime)
}

//-------------------------------------------------------------------------------------------------
// public

func CalculateExpirationTime(request *http.Request) Date {
	now := time.Now().UTC()
	lifetime := nodeLifetime()
	date := Date{Time: now.Add(lifetime)}

	requestDate := request.Header.Get("Date")
	if len(requestDate) == 0 {
		log.Printf("Http request missing \"Date\" header, returning %s expire time", lifetime)
		return date
	}
	requestTime, err := ParseDate(requestDate)
	if err != nil {
		log.Printf("Failed to parse \"Date\" header, returning %s expire time: %s",
			lifetime, err)
		return date
	}
	travelTime := now.Sub(requestTime)
	if travelTime < 0 {
		log.Printf("Expected positive travel time, got: %s", travelTime)
		return date
	}
	date.Time = date.Time.Add(-travelTime)
	return date
}
