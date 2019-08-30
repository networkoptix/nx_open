package main

import (
	"errors"
	"fmt"
	"net/http"
	"os"
)

const kCloudModulesXmlTemplateApi string = `<?xml version="1.0" encoding="utf-8"?>
<sequence>
	<set resName="cdb" resValue="https://%s:443"/>
	<set resName="hpm" resValue="%s"/>
</sequence>
`

const kCloudModulesXmlTemplateV1 string = `<?xml version="1.0" encoding="utf-8"?>
<sequence>
	<set resName="cdb" resValue="https://%s:443"/>
		<sequence>
			<set resName="hpm.tcpUrl" resValue="%s"/>
			<set resName="hpm.udpUrl" resValue="%s"/>
		</sequence>
	<set resName="notification_module" resValue="https://%s:443"/>
	%s
</sequence>
`

const kSpeedTestTemplate string = `<set resName="speedtest_module" resValue="%s"/>`

var CloudModulesXmlFunctions = map[string]func(*http.Request, *Node) (string, error, int){
	"/api/cloud_modules.xml":          ApiCloudModulesXml,
	"/discovery/v1/cloud_modules.xml": V1CloudModulesXml,
}

func speedTestUrl() string {
	return os.Getenv("SPEEDTEST_URL")
}

func ApiCloudModulesXml(request *http.Request, node *Node) (string, error, int /*httpStatusCode*/) {
	cdbHost := request.URL.Query().Get("cdbHost")
	if len(cdbHost) == 0 {
		return "", errors.New(`Http request missing query param "cdbHost"`), http.StatusBadRequest
	}

	infoJson, err := Deserialize(node.InfoJson)
	if err != nil {
		err = errors.New("Error deserializing mediatorInfoJson: " + err.Error())
		return "", err, http.StatusBadRequest
	}
	udpUrl := infoJson.MediatorUrls.Udp
	if len(udpUrl) == 0 {
		return "", errors.New("udp url is empty"), http.StatusBadRequest
	}

	return fmt.Sprintf(kCloudModulesXmlTemplateApi, cdbHost, udpUrl), nil, http.StatusOK
}

func V1CloudModulesXml(request *http.Request, node *Node) (string, error, int /*httpStatusCode*/) {
	cdbHost := request.URL.Query().Get("cdbHost")
	if len(cdbHost) == 0 {
		return "", errors.New(`http request missing query param "cdbHost"`), http.StatusBadRequest
	}

	infoJson, err := Deserialize(node.InfoJson)
	if err != nil {
		err = errors.New("Error deserializing MediatorInfoJson: " + err.Error())
		return "", err, http.StatusBadRequest
	}

	tcpUrl := infoJson.MediatorUrls.Tcp
	udpUrl := infoJson.MediatorUrls.Udp

	if len(tcpUrl) == 0 {
		return "", errors.New("tcp url is empty"), http.StatusBadRequest
	}
	if len(udpUrl) == 0 {
		return "", errors.New("udp url is empty"), http.StatusBadRequest
	}

	speedTestModule := ""
	if url := speedTestUrl(); len(url) > 0 {
		speedTestModule = fmt.Sprintf(kSpeedTestTemplate, url)
	}

	xml := fmt.Sprintf(kCloudModulesXmlTemplateV1, cdbHost, tcpUrl, udpUrl, cdbHost, speedTestModule)
	return xml, nil, http.StatusOK
}
