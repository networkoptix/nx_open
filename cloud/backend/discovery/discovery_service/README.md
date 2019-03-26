## Synchronization Engine Discovery Service written in Go for AWS Lambda, using DynamoDB.

## TODO
 - If requirement to store go packages in $GOPATH is ever lifted, split .go files into their own packages.
 - Integrate with Cmake.


## Dependencies:
- https://github.com/akrylysov/algnhsa
- https://github.com/julienschmidt/httprouter
- https://github.com/aws/aws-lambda-go/lambda
- https://github.com/aws/aws-sdk-go/service/dynamodb
  
To retrieve dependencies:

`go get github.com/akrylysov/algnhsa`  
`go get github.com/julienschmidt/httprouter`  
`go get github.com/aws/aws-lambda-go/lambda`  
`go get github.com/aws/aws-sdk-go/service/dynamodb`  

Note: these dependencies will be stored in `$GOPATH/src`

## Building The Code
There are two lambdas: "node_discovery" (for production) and "node_discovery__test" (for test environment).  
The commands written use "node_discovery" as an example, but it should be replaced with "node_discovery__test" if deploying to the test environment

## Building on Linux (untested)
1. `go build -o node_discovery nx/cloud/discovery`  
2. `zip node_discovery.zip node_discovery`
3. See Deployment section

## Building on Windows

https://docs.aws.amazon.com/lambda/latest/dg/lambda-go-how-to-create-deployment-package.html

1. A special tool is required to package Lambdas for Linux in Windows. to get it:
    - `go get -u github.com/aws/aws-lambda-go/cmd/build-lambda-zip`  
    - It should be installed into the %GOPATH% directory, typically:
        - `%GOPATH%\bin`
    - Use it from inside of %GOPATH%

2. Set the `GOOS` environment variable:  
    - cmd.exe:  
    `set GOOS=linux`  
    - PowerShell:  
    `$env:GOOS = "linux"`

3. Build:
    - If deploying to test environment replace `node_discovery` with `node_discovery__test`
    - `go build -o node_disovery .\nx\cloud\discovery`

4. Create Lambda Deployment Package:  
    - If deploying to test environment replace `node_discovery` with `node_discovery__test`
    - `%USERPROFILE%\go\bin\build-lambda-zip.exe -o node_discovery.zip node_discovery`


## Deployment to AWS
Install AWS CLI: https://docs.aws.amazon.com/cli/latest/userguide/cli-chap-install.html  

Having config and credentials and files simplifies deployment: https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-files.html.

- Sample config file:  
    `[default]`  
    `output = json`  
    `region = us-east-1`  

- Sample credentials file:  
    `[default]`  
    `aws_access_key_id = <YOUR ACCESS KEY ID>`  
    `aws_secret_access_key = <YOUR ACCESS KEY>`

In the following command, `<NODE_DISCOVERY_LAMBDA>` is a place holder. Replace with the name of your lambda. `node_discovery.zip` is the name of the zip file created during building:  
`aws lambda update-function-code --function-name <NODE_DISCOVERY_LAMBDA> --zip-file fileb://node_discovery.zip`

