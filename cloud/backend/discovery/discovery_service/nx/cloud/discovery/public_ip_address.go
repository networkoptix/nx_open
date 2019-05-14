package main

import (
	"log"
	"net"
	"net/http"
	"strings"
)

var parseFunctions = []func(*http.Request) string{
	parseXForwardedForHeader,
	parseForwardedHeader,
	parseRemoteAddr}

func parsePublicIpAddress(request *http.Request) string {
	for _, parseFunc := range parseFunctions {
		if ipAddress := parseFunc(request); len(ipAddress) > 0 {
			return ipAddress
		}
	}
	log.Println("All ip address parse functions failed")
	return ""
}

func parseXForwardedForHeader(request *http.Request) string {
	header := request.Header.Get("X-Forwarded-For")
	if len(header) == 0 {
		log.Println("Empty \"X-Forwarded-For\" header")
		return ""
	}

	ipAddresses := strings.Split(strings.Replace(header, " ", "", -1), ",")
	if len(ipAddresses) > 1 {
		if net.ParseIP(ipAddresses[0]) != nil {
			return ipAddresses[0]
		}
	}

	log.Println("Failed to parse \"X-Forwarded-For\"")
	return ""
}

func parseForwardedHeader(request *http.Request) string {
	extract := func(str string, s1 string, s2 string) string {
		start := strings.Index(str, s1)
		end := strings.Index(str, s2)
		if start == -1 || end == -1 {
			return ""
		}
		return str[start:end]
	}

	// Replacing all uppercase "For=" with lowercase "for="
	header := strings.ToLower(request.Header.Get("Forwarded"))
	if len(header) == 0 {
		log.Println("Empty \"Forwarded\" header")
		return ""
	}

	parts := strings.Split(header, "for=")
	if len(parts) <= 1 {
		log.Printf("\"Forwarded\" header exists but \"for=\" is missing. Full header: %s", header)
		return ""
	}

	// i := 1 because parts[0] will be empty, as the string being split starts with "for="
	for i := 1; i < len(parts); i++ {
		// Dropping semicolon or comma separators
		if semicolon := strings.Index(parts[i], ";"); semicolon != -1 {
			parts[i] = parts[i][:semicolon]
		} else if comma := strings.Index(parts[i], ","); comma != -1 {
			parts[i] = parts[i][:comma]
		}

		// Try ip v4 first
		if ip := net.ParseIP(parts[i]); ip != nil {
			return parts[i]
		}vim 

		// Maybe it's an ip v6 address enclosed in brackets
		if ipV6Str := extract(parts[i], "[", "]"); len(ipV6Str) > 0 {
			if ipV6 := net.ParseIP(ipV6Str); ipV6 != nil {
				return parts[i]
			}
		}

		log.Printf("Failed to parse an ip address from: %s", parts[i])
	}

	log.Println("Failed to parse \"Forwarded\" header")
	return ""
}

func parseRemoteAddr(request *http.Request) string {
	if net.ParseIP(request.RemoteAddr) != nil {
		return request.RemoteAddr
	}

	log.Println("Failed to parse RemoteAddr directly, trying net.SplitHostPort")

	host, _, err := net.SplitHostPort(request.RemoteAddr)
	if err != nil {
		log.Printf("Error from net.SplitHostPort(%s): %s", request.RemoteAddr, err)
		return ""
	}

	if net.ParseIP(host) != nil {
		return host
	}

	log.Println("Failed to parse http.RemoteAddr")
	return ""
}
