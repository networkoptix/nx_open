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

type Item map[string]*dynamodb.AttributeValue

type nodeKey struct {
	nodeId    string
	clusterId string
}

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

func (dao *DAO) getExpiredNodes(clusterId string) ([]nodeKey, error) {
	input := &dynamodb.QueryInput{
		TableName:              tableName(),
		IndexName:              aws.String("clusterId-expirationTime-index"),
		KeyConditionExpression: aws.String("clusterId = :clusterId AND expirationTime <= :now"),
		ExpressionAttributeValues: Item{
			":clusterId": &dynamodb.AttributeValue{S: &clusterId},
			":now":       &dynamodb.AttributeValue{N: toUnixStr(time.Now())},
		},
	}

	var expiredNodes []nodeKey
	doScan := true
	for doScan {
		output, err := dao.db.Query(input)
		if err != nil {
			return nil, err
		}

		for _, item := range output.Items {
			key := nodeKey{
				nodeId:    *item["nodeId"].S,
				clusterId: *item["clusterId"].S,
			}
			expiredNodes = append(expiredNodes, key)
		}

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
		Item: Item{
			"clusterId":      &dynamodb.AttributeValue{S: &clusterId},
			"nodeId":         &dynamodb.AttributeValue{S: &node.NodeId},
			"urls":           &dynamodb.AttributeValue{SS: toStrPtrSlice(node.Urls)},
			"expirationTime": &dynamodb.AttributeValue{N: toUnixStr(node.ExpirationTime.Time)},
			"infoJson":       &dynamodb.AttributeValue{S: &node.InfoJson},
		},
	})

	return err
}

// GetOnlineNodes that have the given clusterId
func (dao *DAO) GetOnlineNodes(clusterId string) ([]Node, error) {
	dao.removeExpiredNodes(clusterId)
	queryInput := &dynamodb.QueryInput{
		TableName:              tableName(),
		IndexName:              aws.String("clusterId-nodeId-index"),
		KeyConditionExpression: aws.String("clusterId = :clusterId"),
		FilterExpression:       aws.String("expirationTime > :now"),
		ExpressionAttributeValues: Item{
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
			nsec, _ := strconv.ParseInt(*item["expirationTime"].N, 10, 64)
			onlineNodes = append(onlineNodes, Node{
				NodeId:         *item["nodeId"].S,
				Urls:           toStrSlice(item["urls"].SS),
				ExpirationTime: Date{Time: time.Unix(0, nsec)},
				InfoJson:       *item["infoJson"].S,
			})
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

	log.Println("expiredNodes len:", len(expiredNodes))

	table := *tableName()
	requestItems := make(map[string][]*dynamodb.WriteRequest)
	for _, n := range expiredNodes {
		key := Item{
			"nodeId":    &dynamodb.AttributeValue{S: aws.String(n.nodeId)},
			"clusterId": &dynamodb.AttributeValue{S: aws.String(n.clusterId)},
		}
		requestItems[table] = append(
			requestItems[table],
			&dynamodb.WriteRequest{DeleteRequest: &dynamodb.DeleteRequest{Key: key}})
	}

	input := &dynamodb.BatchWriteItemInput{RequestItems: requestItems}
	for len(expiredNodes) > 0 {
		output, err := dao.db.BatchWriteItem(input)
		if err != nil {
			log.Println("RemoveExpiredNodes: db.BatchWriteItem error:", err)
			return err
		}
		input.RequestItems = output.UnprocessedItems
	}

	return err
}
