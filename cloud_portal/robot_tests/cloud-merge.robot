*** Settings ***
Resource          resource.robot
Resource          APIresource.robot
Test Setup        Restart
Test Teardown     Remove containers
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

Remove containers
    Stop Container
    Stop Container
    Prune Containers
    Prune Images


Create system and attach to cloud
    [arguments]    ${user}    ${image}    ${port}    ${system name}

    ${cont}    Run Container    ${image}    ${port}
    sleep    5
    ${auth}=    Create List    ${user}    ${password}
    ${default auth}=    Create List    admin    admin
    &{bind json}=    bind system    ${auth}    https://cloud-test.hdw.mx    name=${system name}
    &{Setup Cloud System json}=    Setup Cloud System    ${default auth}    https://localhost:${port}    ${bind json["authKey"]}    ${bind json["name"]}    ${bind json["id"]}    ${bind json["ownerAccountEmail"]}

*** Test Cases ***
# Only one system connected to Cloud Account
#     Create system and attach to cloud    ${EMAIL MERGE OWNER 1}
#     log in    ${EMAIL MERGE OWNER 1}    ${password}
#     Validate Log in
#     Run keyword and expect error    *    Wait until element is visible    ${MERGE BUTTON SYSTEM}    5
#     Stop Container

# 2 Systems: 1 as Owner & 1 as non-Owner
#     Build Image    image1
#     Create system and attach to cloud    ${EMAIL MERGE OWNER 2}    image1
#     Create system and attach to cloud    ${EMAIL MERGE OWNER 1}    image2

From secondary system merge to primary with no other servers
    ${image}    Build Image
    Create system and attach to cloud    ${EMAIL MERGE OWNER 1}    ${image}    7001    API made system 1
    sleep    5
    Create system and attach to cloud    ${EMAIL MERGE OWNER 1}    ${image}    7003    API made system 2
    log in    ${EMAIL MERGE OWNER 1}    ${password}
    Validate Log in
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Verify In System    API made system 2
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}    120
    Wait Until Element Is Visible    ${MERGE BUTTON SYSTEM}
    Click Button    ${MERGE BUTTON SYSTEM}
    Wait Until Elements Are Visible    ${MERGE DIALOG}    ${MERGE X BUTTON}    ${MERGE OK BUTTON}    ${MERGE CANCEL BUTTON}
    Element Text Should Be    ${MERGE SYSTEM DROPDOWN}    API made system 1
