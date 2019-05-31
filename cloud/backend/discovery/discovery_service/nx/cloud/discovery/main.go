package main

import (
	"encoding/json"
	"errors"
	"log"
	"net/http"

	"github.com/akrylysov/algnhsa"
	"github.com/julienschmidt/httprouter"
)

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
		writeError("postNode: db access error", http.StatusInternalServerError, writer, err)
		return
	}

	nodeInfo := &NodeInfo{}
	err = json.NewDecoder(request.Body).Decode(nodeInfo)
	if err != nil {
		writeError("postNode: error deserializing NodeInfo", http.StatusBadRequest, writer, err)
		return
	}

	node := &Node{
		NodeId:          nodeInfo.NodeId,
		InfoJson:        nodeInfo.InfoJson,
		Urls:            nodeInfo.Urls,
		ExpirationTime:  CalculateExpirationTime(request),
		PublicIpAddress: ParsePublicIpAddress(request),
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

func getCloudModulesXmlApi(writer http.ResponseWriter, request *http.Request, params httprouter.Params) {
	onlineNodes, err, httpStatusCode := getOnlineNodesImpl(request.URL.Query().Get("mediatorCluster"))
	if len(onlineNodes) == 0 {
		writeError("getCloudModulesXmlApi: getOnlineNodesImpl", httpStatusCode, writer, err)
		return
	}

	xml, err, httpStatusCode := ApiCloudModulesXml(request, &onlineNodes[0])
	if err != nil {
		writeError("getCloudModulesXmlApi: ApiCloudModulesXml", httpStatusCode, writer, err)
		return
	}

	writer.Header().Set("Content-Type", "application/xml; charset=UTF-8")
	writer.WriteHeader(http.StatusOK)
	writer.Write([]byte(xml))
}

func getCloudModulesXmlV1(writer http.ResponseWriter, request *http.Request, params httprouter.Params) {
	onlineNodes, err, httpStatusCode := getOnlineNodesImpl(request.URL.Query().Get("mediatorCluster"))
	if err != nil {
		writeError("getCloudModulesXmlV1: getOnlineNdoesImpl", httpStatusCode, writer, err)
		return
	}

	xml, err, httpStatusCode := V1CloudModulesXml(request, &onlineNodes[0])
	if err != nil {
		writeError("getCloudModulesXmlV1: V1CloudModulesXml", httpStatusCode, writer, err)
		return
	}

	writer.Header().Set("Content-Type", "application/xml; charset=UTF-8")
	writer.WriteHeader(http.StatusOK)
	writer.Write([]byte(xml))
}

func main() {
	log.Println("start lambda")
	router := httprouter.New()
	router.GET("/cluster/:clusterId/nodes", getOnlineNodes)
	router.POST("/cluster/:clusterId/nodes", postNode)
	router.GET("/api/cloud_modules.xml", getCloudModulesXmlApi)
	router.GET("/discovery/v1/cloud_modules.xml", getCloudModulesXmlV1)
	algnhsa.ListenAndServe(router, nil)
}
