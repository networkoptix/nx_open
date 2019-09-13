package main

import (
	"encoding/json"
	"errors"
	"log"
	"net/http"

	"github.com/akrylysov/algnhsa"
	"github.com/julienschmidt/httprouter"
)

func getCountryCode(apiFunc string, request *http.Request) string {
	const kHeader string = "CloudFront-Viewer-Country"
	countryCode := request.Header.Get(kHeader)
	if len(countryCode) == 0 {
		log.Printf(
			`%s: Warning, http request missing "%s" header. Geolocation will not work\n`,
			apiFunc, kHeader)
	}
	log.Printf(`getCountryCode returning "%s" \n`, countryCode)
	return countryCode
}

func writeError(apiFunc string, httpStatusCode int, writer http.ResponseWriter, err error) {
	log.Printf("%s: error: %s\n", apiFunc, err.Error())
	writer.Header().Set("Content-Type", "text/plain; charset=UTF-8")
	writer.WriteHeader(httpStatusCode)
	if err != nil {
		writer.Write([]byte(err.Error()))
	}
}

func getOnlineNodesImpl(clusterId string) ([]Node, error, int /*httpStatusCode*/) {
	if len(clusterId) == 0 {
		return nil, errors.New("Cluster id not provided"), http.StatusBadRequest
	}

	dao, err := NewDAO()
	if err != nil {
		err = errors.New("Failed to create datbase object: " + err.Error())
		return nil, err, http.StatusInternalServerError
	}

	onlineNodes, err := dao.GetOnlineNodes(clusterId)
	if err != nil {
		err = errors.New("Failed to fetch online nodes from db: " + err.Error())
		return nil, err, http.StatusInternalServerError
	}
	if len(onlineNodes) == 0 {
		err = errors.New("No online nodes matching clusterId: " + clusterId)
		return nil, err, http.StatusNotFound
	}
	return onlineNodes, nil, http.StatusOK
}

func getOnlineNodes(writer http.ResponseWriter, request *http.Request, params httprouter.Params) {
	onlineNodes, err, httpStatusCode := getOnlineNodesImpl(params.ByName("clusterId"))
	if err != nil {
		writeError("getOnlineNodes", httpStatusCode, writer, err)
		return
	}

	writer.Header().Set("Content-Type", "application/json; charset=UTF-8")
	writer.WriteHeader(http.StatusOK)
	err = json.NewEncoder(writer).Encode(onlineNodes) //< Serializing directly into the message body
	if err != nil {
		log.Println("getOnlineNodes: error encoding to json:", err)
	}
}

func postNode(writer http.ResponseWriter, request *http.Request, params httprouter.Params) {
	dao, err := NewDAO()
	if err != nil {
		writeError("postNode: db access", http.StatusInternalServerError, writer, err)
		return
	}

	nodeInfo := &NodeInfo{}
	err = json.NewDecoder(request.Body).Decode(nodeInfo)
	if err != nil {
		writeError("postNode: deserializing NodeInfo", http.StatusBadRequest, writer, err)
		return
	}

	node := &Node{
		NodeId:          nodeInfo.NodeId,
		InfoJson:        nodeInfo.InfoJson,
		Urls:            nodeInfo.Urls,
		ExpirationTime:  CalculateExpirationTime(request),
		PublicIpAddress: ParsePublicIpAddress(request),
		CountryCode:     getCountryCode("postNode", request),
	}
	if len(node.CountryCode) == 0 {
		log.Println("postNode: CountryCode was not found, geopoint location will not work")
	}

	err = dao.InsertOrUpdate(node, params.ByName("clusterId"))
	if err != nil {
		writeError("postNode: dao.insertOrUpdate", http.StatusInternalServerError, writer, err)
		return
	}

	writer.Header().Set("Content-Type", "application/json; charset=UTF-8")
	writer.WriteHeader(http.StatusCreated)     //< Need to write the header before message body
	err = json.NewEncoder(writer).Encode(node) //< Serializing directly into the message body
	if err != nil {
		log.Println("postNode: json encoding error:", err)
	}
}

func getCloudModulesXml(writer http.ResponseWriter, request *http.Request, params httprouter.Params) {
	xmlFunc, ok := CloudModulesXmlFunctions[request.URL.Path]
	if !ok {
		err := errors.New("no cloud_modules.xml function found for url path: " + request.URL.Path)
		writeError("getCloudModulesXml", http.StatusNotFound, writer, err)
		return
	}

	onlineNodes, err, httpStatusCode := getOnlineNodesImpl(request.URL.Query().Get("mediatorCluster"))
	if err != nil {
		writeError("getCloudModulesXml: getOnlineNodesImpl", httpStatusCode, writer, err)
		return
	}

	var node *Node
	clientCountryCode := getCountryCode("getCloudModulesXml", request)
	if len(clientCountryCode) > 0 {
		node = FindNodeClosestToClient(clientCountryCode, onlineNodes)
	}
	if node == nil {
		log.Println("getCloudModulesXml: geolocation failed, using first node in the list")
		node = &onlineNodes[0]
	}

	xml, err, httpStatusCode := xmlFunc(request, node)
	if err != nil {
		apiFunc := "getCloudModulesXml: url path: " + request.URL.Path
		writeError(apiFunc, httpStatusCode, writer, err)
		return
	}

	writer.Header().Set("Content-Type", "application/xml; charset=UTF-8")
	writer.WriteHeader(http.StatusOK)
	writer.Write([]byte(xml))
}

func getVersion(writer http.ResponseWriter, request *http.Request, params httprouter.Params) {
	writer.Header().Set("Content-Type", "application/json; charset=UTF-8")
	json.NewEncoder(writer).Encode(GetVersion()) //< GetVersion is generated from version.go.in by cmake
}

func main() {
	log.Println("start lambda, version: ", GetVersion())
	router := httprouter.New()
	router.GET("/cluster/:clusterId/nodes", getOnlineNodes)
	router.POST("/cluster/:clusterId/nodes", postNode)
	for key := range CloudModulesXmlFunctions {
		router.GET(key, getCloudModulesXml)
	}
	router.GET("/version", getVersion)
	algnhsa.ListenAndServe(router, nil)
}
