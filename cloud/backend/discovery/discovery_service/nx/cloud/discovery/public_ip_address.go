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

func ParsePublicIpAddress(request *http.Request) string {
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

	// ipv4: [\\d\\.]+
	// ipv6: \"\\[(([a-fA-F0-9]*:*)+)\\], match includes  "[  and  ]
	re := regexp.MustCompile("[fF]or=(([\\d\\.]+)|(\"\\[(([a-fA-F0-9]*:*)+)\\]))")
	matches := re.FindAllStringSubmatch(header, 1)
	if len(matches) == 0 || len(matches[0]) == 0 {
		return ""
	}

	ipStr := matches[0][1]
	if strings.HasPrefix(ipStr, "\"[") && strings.HasSuffix(ipStr, "]") {
		ipStr = ipStr[2 : len(ipStr)-1]
	}

	if net.ParseIP(ipStr) != nil {
		return ipStr
	}

	log.Printf("Failed to parse ip address from \"Forwarded\" header: %s", header)
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
