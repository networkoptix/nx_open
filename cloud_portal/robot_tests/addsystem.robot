*** Settings ***
Resource          resource.robot
Library           RequestsLibrary

*** Keywords ***

*** Test Cases ***
Connect system to cloud
    go to web client
    finish server setup with unique name
    add to cloud under owner

Create the Client Custom user type
    &{headers}=  Create Dictionary  Content-Type=application/Json
    ${auth}=    Create List    admin    qweasd123
    ${client custom}    Set Variable    {"name": "mooaw","permissions": "GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission"}
    ${headers}=    Create Dictionary    Content-type    application/json
    Create Session    robot tests    http://localhost:7001    auth=${auth}
    Post Request    robot tests    /ec2/saveUserRole    headers=${headers}    data=${client custom}
Add all required users to system
    verify system is present
    add users under their correct permissions to system