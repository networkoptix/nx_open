*** Settings ***
Resource          resource.robot
Resource          APIresource.robot
Test Setup        Restart
Test Teardown     Remove containers
Suite Setup       Startup
Suite Teardown    Close All Browsers
Force Tags        Threaded

*** Variables ***
${email}             ${EMAIL OWNER}
${password}          ${BASE PASSWORD}
${url}               ${ENV}
${email admin no reg}    noptixautoqa+adminunreg@gmail.com
${email viewer no reg}    noptixautoqa+viewerunreg@gmail.com
${email client custom no reg}    noptixautoqa+clientcustomunreg@gmail.com
&{users dict 1}      cloudAdmin=${EMAIL ADMIN}    Viewer=${EMAIL VIEWER}    Custom=${EMAIL CLIENT CUSTOM}
&{users dict 2}      cloudAdmin=${email admin no reg}     Viewer=${email viewer no reg}    Custom=${email client custom no reg}
&{all users dict}    Administrator=${EMAIL ADMIN}    Viewer=${EMAIL VIEWER}    ClientCustom=${EMAIL CLIENT CUSTOM}
...                  Administrator=${email admin no reg}     Viewer=${email viewer no reg}    ClientCustom=${email client custom no reg}

*** Keywords ***
Merge
    [arguments]    ${primary}    ${target}    ${expected dropdown}
    Wait Until Element Is Visible    ${MERGE BUTTON SYSTEM}
    Click Button    ${MERGE BUTTON SYSTEM}
    Wait Until Elements Are Visible    ${MERGE DIALOG}    ${MERGE X BUTTON}    ${MERGE OK BUTTON}    ${MERGE CANCEL BUTTON}
    Element Text Should Be    ${MERGE SYSTEM DROPDOWN}    ${expected dropdown}
    Click Element    ${MERGE SYSTEM DROPDOWN}
    Wait Until Element Is Visible    ${MERGE FORM}//li/a//span[text()="${target}"]
    Click Element    ${MERGE FORM}//li/a//span[text()="${target}"]
    ${radio button}    Radio Button    ${primary}
    Wait Until Element Is Visible   ${radio button}
    Click Element    ${radio button}
    Wait Until Element Is Visible    ${radio button}/span[contains(@class, "checked")]
    sleep    5
    Click Button    ${MERGE OK BUTTON}
    Wait Until Elements Are Visible    ${MERGE BUTTON MODAL}    ${MERGE PASSWORD INPUT}    ${MERGE CANCEL BUTTON}    timeout=60
    Input Text    ${MERGE PASSWORD INPUT}    ${BASE PASSWORD}
    Click Button    ${MERGE BUTTON MODAL}
    sleep    1
    Check For Alert    ${SYSTEM MERGING TEXT}
    Element Should Not Be Visible    ${MERGE DIALOG}
    Wait Until Element Is Visible    ${CURRENTLY MERGING CARD}

Prepare System
    [arguments]    ${user}    ${auth}    ${image}    ${port}    ${system name}    ${users dict}    ${network}=bridge
    ${system id}    Create system and attach to cloud    ${user}    ${image}    ${port}    ${system name}   network=${network}
    Sleep    5
    Add users to system    ${auth}    ${users dict}    ${system id}
    &{Save User role json}=    Save User Role    ${auth}    https://localhost:${port}    ClientCustom    GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission
    Set user to client custom    ${auth}    ${port}    ${Save User role json["id"]}

Create system and attach to cloud
    [arguments]    ${user}    ${image}    ${port}    ${system name}    ${network}=bridge
    ${cont}    Run Container    ${image}    ${port}    ${network}
    sleep    5
    ${auth}=    Create List    ${user}    ${password}
    ${default auth}=    Create List    admin    admin
    &{bind json}=    bind system    ${auth}    ${ENV}    name=${system name}
    &{Setup Cloud System json}=    Setup Cloud System    ${default auth}    https://localhost:${port}    ${bind json["authKey"]}    ${bind json["name"]}    ${bind json["id"]}    ${bind json["ownerAccountEmail"]}
    [return]    ${bind json["id"]}

Add users to system
    [arguments]    ${auth}    ${users}    ${system id}
    FOR     ${key}    IN    @{users.keys()}
        Share    ${auth}    ${system id}    ${key}    ${users["${key}"]}
    END

Set user to client custom
    [arguments]    ${auth}    ${port}    ${role id}
    FOR    ${index}    IN RANGE    100
        @{users list}=    Get Users    ${auth}    https://localhost:${port}
        ${length}    Get Length     ${users list}
        Exit For Loop If    ${length}>3
        Sleep    2
    END

    FOR    ${user}    IN    @{users list}
        ${user id}    Set Variable    ${user["id"]}
        log    ${user["email"]}
        Exit For Loop If    "${user["email"]}"=="${EMAIL CLIENT CUSTOM}" or "${user["email"]}"=="noptixautoqa+clientcustomunreg@gmail.com"
    END
    &{Save User json}=    Save User    ${auth}    https://localhost:${port}    ${user id}    ${role id}

Radio Button
    [arguments]    ${text}
    [return]    //form[@name="mergeForm"]//input[@type="radio"]/parent::label[contains(text(), "${text}")]

Remove containers
    Stop Containers
    Prune Containers
    Remove Images

Startup
    Open Browser and go to URL    ${url}
    ${image}    Build Image
    Set Suite Variable    ${image}    ${image}

Restart
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}
    Validate Log Out

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

From secondary system merge to primary with no other systems
    ${user}    Set Variable    ${EMAIL MERGE OWNER 2}
    ${auth}=    Create List    ${user}    ${password}
    Prepare System    ${user}    ${auth}    ${image}    7001    API made system 1    ${users dict 1}    network=host
    Prepare System    ${user}    ${auth}    ${image}    7003    API made system 2    ${users dict 2}
    log in    ${user}    ${password}
    Validate Log in
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}    180
    Go To    ${url}/systems
    Validate Log In
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Verify In System    API made system 2
    Capture Page Screenshot    system2.png
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}    180

    Merge    API made system 1    API made system 1    API made system 1

    Check for alert    Connection to API made system 2 lost    timeout=120
    Verify In System    API made system 1
    Wait Until Element Is Visible    ${USERS LIST}
    Wait Until Element Is Not Visible    ${CURRENTLY MERGING CARD}    120
    FOR     ${idx}  IN RANGE  50
        ${result}    Run Keyword And Ignore Error    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${email admin no reg}')]
        Reload Page
        Exit For Loop If    '${result[0]}'=='PASS'
    END
    FOR    ${key}  IN  @{all users dict.keys()}
        Check User Permissions    ${all users dict["${key}"]}    ${key}    timeout=120
    END
    Disconnect from cloud
