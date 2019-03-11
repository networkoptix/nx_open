*** Settings ***
Resource          resource.robot
Library           RequestsLibrary

*** Variables ***
${sysname}   Auto Tests

*** Keywords ***

*** Test Cases ***
Connect system to cloud
    ${auth}=    Create List    admin    qweasd123
    ${client custom}    Set Variable    {"systemName": "${sysname}","cloudAuthKey": "", "cloudSystemID":""}
    ${headers}=    Create Dictionary    Content-type    application/json
    Create Session    robot tests    http://localhost:7001    auth=${auth}
    ${resp}=    Post Request    robot tests    /api/setupCloudSystem    headers=${headers}    data=${client custom}
    Should Be Equal As Strings  ${resp.status_code}  200

Create the Client Custom user type
    ${auth}=    Create List    admin    qweasd123
    ${client custom}    Set Variable    {"name": "mooaw","permissions": "GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission"}
    ${headers}=    Create Dictionary    Content-type    application/json
    Create Session    robot tests    http://localhost:7001    auth=${auth}
    ${resp}=    Post Request    robot tests    /ec2/saveUserRole    headers=${headers}    data=${client custom}
    Should Be Equal As Strings  ${resp.status_code}  200

#Add all required users to system
#    verify system is present
#    add users under their correct permissions to system