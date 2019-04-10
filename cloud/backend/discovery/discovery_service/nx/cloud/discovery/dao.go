package main

import (
	"log"
	"os"
	"strconv"
	"time"

	"github.com/aws/aws-sdk-go/aws"
	"github.com/aws/aws-sdk-go/aws/session"
	"github.com/aws/aws-sdk-go/service/dynamodb"
)

func tableName() *string {
	return aws.String(os.Getenv("DISCOVERY_TABLE_NAME"))
}

func toStrPtrSlice(strSlice []string) []*string {
	strPtrSlice := make([]*string, len(strSlice))
	for i, _ := range strSlice {
		strPtrSlice[i] = &strSlice[i]
	}
	return strPtrSlice
}

func toStrSlice(strPtrSlice []*string) []string {
	strSlice := make([]string, len(strPtrSlice))
	for i, _ := range strPtrSlice {
		strSlice[i] = *strPtrSlice[i]
	}
	return strSlice
}

func toUnixStr(t time.Time) *string {
	str := strconv.FormatInt(t.UnixNano(), 10)
	return &str
}

func toNode(item map[string]*dynamodb.AttributeValue) Node {
	nsec, _ := strconv.ParseInt(*item["expirationTime"].N, 10, 64)
	node := Node{
		NodeId:         *item["nodeId"].S,
		ExpirationTime: Date{Time: time.Unix(0, nsec)},
	}
	if urls, ok := item["urls"]; ok {
		node.Urls = toStrSlice(urls.SS)
	}
	if infoJson, ok := item["infoJson"]; ok {
		node.InfoJson = *infoJson.S
	}
	return node
}

func toItem(node *Node, clusterId string) map[string]*dynamodb.AttributeValue {
	item := map[string]*dynamodb.AttributeValue{
		"clusterId":      &dynamodb.AttributeValue{S: &clusterId},
		"nodeId":         &dynamodb.AttributeValue{S: &node.NodeId},
		"expirationTime": &dynamodb.AttributeValue{N: toUnixStr(node.ExpirationTime.Time)},
	}
	if len(node.Urls) > 0 {
		item["urls"] = &dynamodb.AttributeValue{SS: toStrPtrSlice(node.Urls)}
	}
	if len(node.InfoJson) > 0 {
		item["infoJson"] = &dynamodb.AttributeValue{S: &node.InfoJson}
	}
	return item
}

func (dao *DAO) getExpiredNodes(clusterId string) ([]map[string]*dynamodb.AttributeValue, error) {
	input := &dynamodb.QueryInput{
		TableName:              tableName(),
		IndexName:              aws.String("clusterId-expirationTime-index"),
		KeyConditionExpression: aws.String("clusterId = :clusterId AND expirationTime <= :now"),
		ExpressionAttributeValues: map[string]*dynamodb.AttributeValue{
			":clusterId": &dynamodb.AttributeValue{S: &clusterId},
			":now":       &dynamodb.AttributeValue{N: toUnixStr(time.Now())},
		},
		ProjectionExpression: aws.String("nodeId, clusterId"),
	}

	var expiredNodes []map[string]*dynamodb.AttributeValue
	doScan := true
	for doScan {
		output, err := dao.db.Query(input)
		if err != nil {
			return nil, err
		}

		expiredNodes = append(expiredNodes, output.Items...)

		input.ExclusiveStartKey = output.LastEvaluatedKey
		doScan = len(output.LastEvaluatedKey) > 0
	}

	return expiredNodes, nil
}

//-------------------------------------------------------------------------------------------------
// public

// DAO is a Data Access Object providing access to persistent storage
type DAO struct {
	db *dynamodb.DynamoDB
}

// NewDAO constructs and return a pointer to a new DAO
func NewDAO() (*DAO, error) {
	sess, err := session.NewSession(&aws.Config{
		Region: aws.String("us-east-1")},
	)
	if err != nil {
		return nil, err
	}

	dao := &DAO{}
	dao.db = dynamodb.New(sess)

	return dao, nil
}

// InsertOrUpdate the given Node with the given clusterId
func (dao *DAO) InsertOrUpdate(node *Node, clusterId string) error {
	_, err := dao.db.PutItem(&dynamodb.PutItemInput{
		TableName: tableName(),
		Item:      toItem(node, clusterId),
	})

	return err
}

// GetOnlineNodes that have the given clusterId
func (dao *DAO) GetOnlineNodes(clusterId string) ([]Node, error) {
	dao.removeExpiredNodes(clusterId)

	queryInput := &dynamodb.QueryInput{
		TableName:              tableName(),
		IndexName:              aws.String("clusterId-expirationTime-index"),
		KeyConditionExpression: aws.String("clusterId = :clusterId AND expirationTime > :now"),
		ExpressionAttributeValues: map[string]*dynamodb.AttributeValue{
			":clusterId": &dynamodb.AttributeValue{S: &clusterId},
			":now":       &dynamodb.AttributeValue{N: toUnixStr(time.Now())},
		},
		ProjectionExpression: aws.String("nodeId, urls, expirationTime, infoJson"),
	}

	var onlineNodes []Node
	doQuery := true
	for doQuery {
		output, err := dao.db.Query(queryInput)
		if err != nil {
			return nil, err
		}

		for _, item := range output.Items {
			onlineNodes = append(onlineNodes, toNode(item))
		}

		queryInput.ExclusiveStartKey = output.LastEvaluatedKey
		doQuery = len(output.LastEvaluatedKey) > 0
	}

	return onlineNodes, nil
}

func (dao *DAO) removeExpiredNodes(clusterId string) error {
	expiredNodes, err := dao.getExpiredNodes(clusterId)
	if err != nil {
		log.Println("RemoveExpiredNodes: error fetching list of expired nodes:", err)
		return err
	}

	requestItems := make(map[string][]*dynamodb.WriteRequest)
	for _, item := range expiredNodes {
		requestItems[*tableName()] = append(
			requestItems[*tableName()],
			&dynamodb.WriteRequest{DeleteRequest: &dynamodb.DeleteRequest{Key: item}})
	}

	input := &dynamodb.BatchWriteItemInput{RequestItems: requestItems}
	for len(input.RequestItems) > 0 {
		output, err := dao.db.BatchWriteItem(input)
		if err != nil {
			log.Println("RemoveExpiredNodes: db.BatchWriteItem error:", err)
			return err
		}
		input.RequestItems = output.UnprocessedItems
	}

	return err
}
