package main

// NodeInfo clients send this structure to the discovery service to register
type NodeInfo struct {
	NodeId   string   `json:"nodeId"`
	Urls     []string `json:"urls"`
	InfoJson string   `json:"infoJson"`
}

// Node represents a discovered node, sent back to client in response to registration
type Node struct {
	NodeId         string   `json:"nodeId"`
	Urls           []string `json:"urls"`
	ExpirationTime Date     `json:"expirationTime"`
	InfoJson       string   `json:"infoJson"`
}
