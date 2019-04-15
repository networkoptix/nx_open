package main

import (
	"encoding/json"
	"errors"
	"log"
	"net/http"
	"time"

	"github.com/akrylysov/algnhsa"
	"github.com/julienschmidt/httprouter"
)

func calculateExpirationTime(request *http.Request) Date {
	now := time.Now().UTC()
	minutes := 5
	date := Date{Time: now.Add(time.Minute * time.Duration(minutes))}

	requestDate := request.Header.Get("Date")
	if len(requestDate) == 0 {
		log.Printf(
			"Http request missing \"Date\" header, returning %d minute expire time",
			minutes)
		return date
	}
	requestTime, err := ParseDate(requestDate)
	if err != nil {
		log.Printf(
			"Failed to parse \"Date\" header, returning %d minute expire time: %s",
			minutes, err)
		return date
	}
	travelTime := now.Sub(requestTime)
	if travelTime < 0 {
		const nsecToMsec = 1000000
		log.Printf("Expected positive travel time, got: %d millieconds", travelTime/nsecToMsec)
		return date
	}
	date.Time = date.Time.Add(-travelTime)
	return date
}

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
		NodeId:         nodeInfo.NodeId,
		InfoJson:       nodeInfo.InfoJson,
		Urls:           nodeInfo.Urls,
		ExpirationTime: calculateExpirationTime(request),
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
