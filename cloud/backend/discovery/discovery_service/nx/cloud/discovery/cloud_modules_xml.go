package main

import (
	"errors"
	"fmt"
	"log"
	"net/http"
)

var kCloudModulesXmlTemplateApi = `<?xml version="1.0" encoding="utf-8"?>
<sequence>
	<set resName="cdb" resValue="https://%s:443"/>
	<set resName="hpm" resValue="%s"/>
	<set resName="notification_module" resValue="https://%s:443"/>
</sequence>
`

var kCloudModulesXmlTemplateV1 = `<?xml version="1.0" encoding="utf-8"?>
<sequence>
	<set resName="cdb" resValue="https://%s:443"/>
		<sequence>
			<set resName="hpm.tcpUrl" resValue="%s"/>
			<set resName="hpm.udpUrl" resValue="%s"/>
		</sequence>
	<set resName="notification_module" resValue="https://%s:443"/>
</sequence>
`

func ApiCloudModulesXml(request *http.Request, node *Node) (string, error) {
	cdbHost := request.URL.Query().Get("cdbHost")
	if len(cdbHost) == 0 {
		return "", errors.New(`http request missing query param "cdbHost"`)
	}

	infoJson, err := Deserialize(node.InfoJson)
	if err != nil {
		log.Println("Error deserializing mediatorInfoJson:", err.Error())
		return "", err
	}
	udpUrl := infoJson.MediatorUrls.Udp
	if len(udpUrl) == 0 {
		log.Println("Error: udp url is empty")
		return "", errors.New("udp url is empty")
	}

	return fmt.Sprintf(kCloudModulesXmlTemplateApi, cdbHost, udpUrl, cdbHost), nil
}

func V1CloudModulesXml(request *http.Request, node *Node) (string, error) {
	cdbHost := request.URL.Query().Get("cdbHost")
	if len(cdbHost) == 0 {
		return "", errors.New(`http request missing query param "cdbHost"`)
	}

	infoJson, err := Deserialize(node.InfoJson)
	if err != nil {
		log.Println("Error deserializing mediatorInfoJson:", err.Error())
		return "", err
	}

	tcpUrl := infoJson.MediatorUrls.Tcp
	udpUrl := infoJson.MediatorUrls.Udp

	if len(tcpUrl) == 0 {
		log.Println("Error: tcp url is empty")
		return "", errors.New("tcp url is empty")
	}
	if len(udpUrl) == 0 {
		log.Println("Error: tcp url is empty")
		return "", errors.New("udp url is empty")
	}

	return fmt.Sprintf(kCloudModulesXmlTemplateV1, cdbHost, tcpUrl, udpUrl, cdbHost), nil
}
