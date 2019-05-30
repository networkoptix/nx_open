package main

import (
	"encoding/json"
	"errors"
	"log"
	"net/http"

	"github.com/akrylysov/algnhsa"
	"github.com/julienschmidt/httprouter"
)

func writeError(httpStatusCode int, writer http.ResponseWriter, err error) {
	writer.Header().Set("Content-Type", "text/plain; charset=UTF-8")
	writer.WriteHeader(httpStatusCode)
	if err != nil {
		writer.Write([]byte(err.Error()))
	}
}

func getOnlineNodes(writer http.ResponseWriter, request *http.Request, params httprouter.Params) {
	dao, err := NewDAO()
	if err != nil {
		log.Println("getOnlineNodes: error connecting to db:", err)
		writeError(http.StatusInternalServerError, writer, err)
		return
	}

	onlineNodes, err := dao.GetOnlineNodes(params.ByName("clusterId"))
	if err != nil {
		log.Println("getOnlineNodes: error fetching online nodes from db:", err,
			", clusterId:", params.ByName("clusterId"))
		writeError(http.StatusInternalServerError, writer, err)
		return
	}

	if len(onlineNodes) == 0 {
		errStr := "getOnlineNodes: no nodes matching clusterId: " + params.ByName("clusterId")
		writeError(http.StatusNotFound, writer, errors.New(errStr))
		return
	}

	writer.Header().Set("Content-Type", "application/json; charset=UTF-8")
	writer.WriteHeader(http.StatusOK)
	json.NewEncoder(writer).Encode(onlineNodes) //< Serializing directly into the message body
	if err != nil {
		log.Println("getOnlineNodes: error encoding to json:", err)
	}
}

func postNode(writer http.ResponseWriter, request *http.Request, params httprouter.Params) {
	dao, err := NewDAO()
	if err != nil {
		log.Println("postNode: db access error: ", err)
		writeError(http.StatusInternalServerError, writer, err)
		return
	}

	nodeInfo := &NodeInfo{}
	err = json.NewDecoder(request.Body).Decode(nodeInfo)
	if err != nil {
		log.Println("postNode: Info json decoding error:", err)
		writeError(http.StatusBadRequest, writer, err)
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
		log.Println("postNode: db insertOrUpdate error:", err, ", with node:", node)
		writeError(http.StatusInternalServerError, writer, err)
		return
	}

	writer.Header().Set("Content-Type", "application/json; charset=UTF-8")
	writer.WriteHeader(http.StatusCreated)     //< Need to write the header before message body
	err = json.NewEncoder(writer).Encode(node) //< Serializing directly into the message body
	if err != nil {
		log.Println("postNode: error encoding to json:", err)
	}
}

func main() {
	log.Println("start lambda")
	router := httprouter.New()
	router.GET("/cluster/:clusterId/nodes", getOnlineNodes)
	router.POST("/cluster/:clusterId/nodes", postNode)
	algnhsa.ListenAndServe(router, nil)
}
