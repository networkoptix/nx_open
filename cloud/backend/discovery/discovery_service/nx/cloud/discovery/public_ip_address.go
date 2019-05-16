package main

import (
	"log"
	"net"
	"net/http"
	"regexp"
	"strings"
)

var parseFunctions = []func(*http.Request) string{
	parseXForwardedForHeader,
	parseForwardedHeader,
	parseRemoteAddr}

func parseXForwardedForHeader(request *http.Request) string {
	header := request.Header.Get("X-Forwarded-For")
	if len(header) == 0 {
		log.Println("Empty \"X-Forwarded-For\" header")
		return ""
	}

	ipAddresses := strings.Split(header, ",")
	if len(ipAddresses) > 1 {
		ip := strings.Trim(ipAddresses[0], " ")
		if net.ParseIP(ip) != nil {
			return ip
		}
	}

	log.Printf("Failed to parse \"X-Forwarded-For\" header: %s", header)
	return ""
}

func parseForwardedHeader(request *http.Request) string {
	header := request.Header.Get("Forwarded")
	if len(header) == 0 {
		log.Println("Empty \"Forwarded\" header")
		return ""
	}

	// ipv4: [fF]or=([\\d\\.]+)
	// ipv6: [fF]or=\"?\\[([a-fA-F0-9:]*)\\][:\\d]*\"?
	re := regexp.MustCompile("[fF]or=([\\d\\.]+)|[fF]or=\"?\\[([a-fA-F0-9:]*)\\][:\\d]*\"?")
	matches := re.FindStringSubmatch(header)
	if len(matches) < 2 {
		log.Println("Invalid expression: ", header)
		return ""
	}

	ipStr := matches[1] //< ipv4 location
	if ipStr == "" {
		ipStr = matches[2] //< ipv6 location
	}

	if net.ParseIP(ipStr) != nil {
		return ipStr
	}

	log.Println("Failed to parse ip address from \"Forwarded\" header: ", header)
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

//-------------------------------------------------------------------------------------------------
// public

func ParsePublicIpAddress(request *http.Request) string {
	for _, parseFunc := range parseFunctions {
		if ipAddress := parseFunc(request); len(ipAddress) > 0 {
			return ipAddress
		}
	}
	log.Println("All ip address parse functions failed")
	return ""
}
