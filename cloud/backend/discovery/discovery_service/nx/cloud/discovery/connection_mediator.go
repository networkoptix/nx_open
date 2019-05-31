package main

import (
	"encoding/json"
)

type Urls struct {
	Tcp string `json:"tcp"`
	Udp string `json:"udp"`
}

type MediatorInfoJson struct {
	MediatorUrls Urls `json:"mediatorUrls"`
}

func Deserialize(infoJson string) (MediatorInfoJson, error) {
	var mediatorInfoJson MediatorInfoJson
	err := json.Unmarshal([]byte(infoJson), &mediatorInfoJson)
	return mediatorInfoJson, err
}
