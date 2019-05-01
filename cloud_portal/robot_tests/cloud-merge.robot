*** Settings ***
Resource          resource.robot
Resource          APIresource.robot
Test Setup        Restart
#Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers
Force Tags        Threaded

*** Variables ***
${email}             ${EMAIL OWNER}
${password}          ${BASE PASSWORD}
${url}               ${ENV}
${other packages}    //div[contains(@class,"card-body")]//div[contains(@class, "installers")]//a

*** Keywords ***
Restart
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}
    Validate Log Out

Create system and attach to cloud
    ${image}    Build Image
    ${cont}    Run Container    ${image}
    ${auth}=    Create List    ${EMAIL OWNER}    ${password}
    ${default auth}=    Create List    admin    admin
    &{bind json}=    bind system    ${auth}    https://cloud-test.hdw.mx
    &{Setup Cloud System json}=    Setup Cloud System    ${default auth}    https://localhost:7001    ${bind json["authKey"]}    ${bind json["name"]}    ${bind json["id"]}    ${bind json["ownerAccountEmail"]}
    Stop Container    ${cont}

*** Test Cases ***
Only one system connected to Cloud Account
    Create system and attach to cloud