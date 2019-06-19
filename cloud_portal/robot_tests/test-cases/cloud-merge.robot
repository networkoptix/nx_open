*** Settings ***
Resource          ../resource.robot
Resource          ../APIresource.robot
Suite Setup       Startup
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    reset state
Suite Teardown    Remove Containers
Force Tags        Threaded    merge

*** Variables ***
${email}             ${EMAIL OWNER}
${password}          ${BASE PASSWORD}
${url}               ${ENV}
${email admin no reg}    noptixautoqa+adminunreg@gmail.com
${email viewer no reg}    noptixautoqa+viewerunreg@gmail.com
${email client custom no reg}    noptixautoqa+clientcustomunreg@gmail.com
&{users dict 1}      cloudAdmin=${EMAIL ADMIN}    Viewer=${EMAIL VIEWER}    Custom=${EMAIL CLIENT CUSTOM}    liveViewer=${EMAIL LIVE VIEWER}
&{users dict 2}      cloudAdmin=${email admin no reg}     Viewer=${email viewer no reg}    Custom=${email client custom no reg}    liveViewer=${EMAIL LIVE VIEWER}
&{all users dict}    Administrator=${EMAIL ADMIN}    Viewer=${EMAIL VIEWER}    ClientCustom=${EMAIL CLIENT CUSTOM}
...                  Administrator=${email admin no reg}     Viewer=${email viewer no reg}    ClientCustom=${email client custom no reg}

*** Keywords ***
Merge
    [arguments]    ${primary}    ${target}    ${expected dropdown}
    Wait Until Element Is Visible    ${MERGE BUTTON SYSTEM}
    Click Button    ${MERGE BUTTON SYSTEM}
    Wait Until Elements Are Visible    ${MERGE DIALOG}    ${MERGE X BUTTON}    ${MERGE OK BUTTON}
    ...                                ${MERGE CANCEL BUTTON}    ${MERGE CURRENT SYSTEM WITH}    ${MERGE ONLY AS OWNER}
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
    Wait Until Element Is Visible    ${MERGE CHECKING HINT}
    Wait Until Elements Are Visible    ${MERGE BUTTON MODAL}    ${MERGE PASSWORD INPUT}    ${MERGE CANCEL BUTTON}
    Input Text    ${MERGE PASSWORD INPUT}    ${BASE PASSWORD}
    Click Button    ${MERGE BUTTON MODAL}
    sleep    1

Validate Merge
    Check For Alert    ${SYSTEM MERGING TEXT}
    Element Should Not Be Visible    ${MERGE DIALOG}
    Wait Until Elements Are Visible    ${CURRENTLY MERGING CARD}    ${CURRENTLY MERGING DOTS}

Validate system available
    [arguments]    ${system name}
    Verify In System    ${system name}
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    ${elements}    Set Variable    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    FOR    ${element}    IN    @{elements}
        Wait Until Element Is Enabled    ${element}    180
    END

Prepare System With Users
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
    Close All Browsers

Startup
    Open Browser and go to URL    ${url}
    ${image}    Build Image
    Set Suite Variable    ${image}    ${image}

Restart
    Stop containers
    Prune Containers
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}
    Validate Log Out

Reset state
    Stop Containers
    Prune Containers
    Close Browser
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL MERGE OWNER 1}    ${password}
    Validate Log In
    ${state}    Run Keyword And Ignore Error    Element Should Be Visible    ${YOU HAVE NO SYSTEMS}
    ${count}    Run Keyword And Ignore Error    Get Element Count    ${SYSTEMS TILE}
    FOR    ${idx}    IN RANGE   ${count}[1]-1
        Exit For Loop If    "${state[0]}"=="PASS"
        Click Element    ${SYSTEMS TILE}
        Disconnect from cloud
    END
    Run Keyword Unless    "${state[0]}"=="PASS"    Disconnect from cloud
    Log Out
    Validate Log Out
    Log In    ${EMAIL MERGE OWNER 2}    ${password}
    Validate Log In
    ${count}    Run Keyword And Ignore Error    Get Element Count    ${SYSTEMS TILE}
    FOR    ${idx}    IN RANGE   ${count}[1]-1
        Click Element    ${SYSTEMS TILE}
        Disconnect from cloud
    END
    Run Keyword Unless    "${state[0]}"=="PASS"    Disconnect from cloud
    Log Out
    Validate Log Out
    Log In    ${EMAIL MERGE OWNER 3.0}    ${password}
    Validate Log In
    ${state}    Run Keyword And Ignore Error    Wait Until Element Is Visible    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]    10
    Run Keyword If    "${state[0]}"=="PASS"    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Run Keyword If    "${state[0]}"=="PASS"    Disconnect from cloud


*** Test Cases ***
Wrong and empty password
    [tags]    C54685
    ${user}    Set Variable    ${EMAIL MERGE OWNER 2}
    ${auth}=    Create List    ${user}    ${password}
    Create system and attach to cloud    ${user}    ${image}    7001    API made system 1
    Create system and attach to cloud    ${user}    ${image}    7003    API made system 2
    log in    ${user}    ${password}
    Validate Log in
    Wait Until Element Is Visible    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Validate system available    API made system 1
    Go To    ${url}/systems
    Validate Log In
    Wait Until Element Is Visible    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Validate system available    API made system 2
    Wait Until Element Is Visible    ${MERGE BUTTON SYSTEM}
    Click Button    ${MERGE BUTTON SYSTEM}
    Wait Until Elements Are Visible    ${MERGE DIALOG}    ${MERGE X BUTTON}    ${MERGE OK BUTTON}
    ...                                ${MERGE CANCEL BUTTON}    ${MERGE CURRENT SYSTEM WITH}    ${MERGE ONLY AS OWNER}
    Click Button    ${MERGE OK BUTTON}
    Wait Until Element Is Visible    ${MERGE CHECKING HINT}
    Wait Until Elements Are Visible    ${MERGE BUTTON MODAL}    ${MERGE PASSWORD INPUT}    ${MERGE CANCEL BUTTON}
    Click Button    ${MERGE BUTTON MODAL}
    Wait Until Element Is Visible    ${MERGE PASSWORD REQUIRED}
    Input Text    ${MERGE PASSWORD INPUT}    qwerasdf
    Click Button    ${MERGE BUTTON MODAL}
    Wait Until Element Is Visible    ${MERGE PASSWORD INCORRECT}
    Press Key    ${MERGE BUTTON MODAL}    ${ESCAPE}
    Disconnect from cloud
    Disconnect from cloud

Only one system connected to Cloud Account
    ${user}    Set Variable    ${EMAIL MERGE OWNER 1}
    ${auth}=    Create List    ${user}    ${password}
    Create system and attach to cloud    ${user}    ${image}    7001    API made system 1
    log in    ${EMAIL MERGE OWNER 1}    ${password}
    Validate Log in
    Run keyword and expect error    *    Wait until element is visible    ${MERGE BUTTON SYSTEM}    5
    Disconnect from cloud

2 Systems: 1 as Owner & 1 as non-Owner
    Create system and attach to cloud    ${EMAIL MERGE OWNER 1}    ${image}    7001    API made system 1
    Create system and attach to cloud    ${EMAIL MERGE OWNER 2}    ${image}    7003    API made system 2
    log in    ${EMAIL MERGE OWNER 1}    ${password}
    Validate Log in
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}    180
    Share To    ${EMAIL MERGE OWNER 2}    Administrator
    Log Out
    Log In    ${EMAIL MERGE OWNER 2}    ${password}
    Validate Log In
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Validate system available    API made system 2
    Wait Until Element Is Visible    ${MERGE BUTTON SYSTEM}
    Wait Until Element Is Enabled    ${MERGE BUTTON SYSTEM}    180
    Click Button    ${MERGE BUTTON SYSTEM}
    Sleep    2
    Get Text    ${MERGE DIALOG}//p[contains(text(),"Connect System you want to merge with this one to")]
    Wait Until Elements Are Visible    ${MERGE NOT OWNER MESSAGE 2}    ${MERGE DIALOG}//p[contains(text(),'${MERGE NOT OWNER MESSAGE 1 TEXT}')]    ${MERGE OK BUTTON}    ${MERGE X BUTTON}
    Element Text Should Be    ${MERGE NOT OWNER MESSAGE 2}    ${MERGE NOT OWNER MESSAGE 2 TEXT}
    Click Button    ${MERGE OK BUTTON}
    Wait Until Element Is Not Visible    ${MERGE DIALOG}

    Click Button    ${MERGE BUTTON SYSTEM}
    Sleep    2
    Wait Until Elements Are Visible    ${MERGE NOT OWNER MESSAGE 2}    ${MERGE DIALOG}//p[contains(text(),'${MERGE NOT OWNER MESSAGE 1 TEXT}')]    ${MERGE OK BUTTON}    ${MERGE X BUTTON}
    Click Button    ${MERGE X BUTTON}
    Wait Until Element Is Not Visible    ${MERGE DIALOG}

    Click Button    ${MERGE BUTTON SYSTEM}
    Sleep    2
    Wait Until Elements Are Visible    ${MERGE NOT OWNER MESSAGE 2}    ${MERGE DIALOG}//p[contains(text(),'${MERGE NOT OWNER MESSAGE 1 TEXT}')]    ${MERGE OK BUTTON}    ${MERGE X BUTTON}
    Press Key    ${MERGE OK BUTTON}    ${ESCAPE}
    Wait Until Element Is Not Visible    ${MERGE DIALOG}

    Disconnect from cloud
    Log Out
    Validate Log Out
    Log In    ${EMAIL MERGE OWNER 1}    ${password}
    Validate Log In
    Disconnect from cloud

2 Systems: 1 online and 1 offline
    [tags]    C53960
    ${user}    Set Variable    ${EMAIL MERGE OWNER 1}
    ${auth}=    Create List    ${user}    ${password}
    Create system and attach to cloud    ${user}    ${image}    7001    API made system 1
    Stop Containers
    Prune Containers
    Create system and attach to cloud    ${user}    ${image}    7003    API made system 2
    log in    ${user}    ${password}
    Validate Log in
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Validate system available    API made system 2
    Wait Until Element Is Visible    ${MERGE BUTTON SYSTEM}
    Wait Until Element Is Enabled    ${MERGE BUTTON SYSTEM}    180
    Click Button    ${MERGE BUTTON SYSTEM}
    Wait until elements are visible    ${MERGE FORM}    ${MERGE SYSTEM DROPDOWN}    ${MERGE OK BUTTON}    ${MERGE CANCEL BUTTON}
    Click Button    ${MERGE OK BUTTON}
    ${error message}    Replace String    ${CANNOT MERGE WITH OFFLINE SYSTEM TEXT}    %SYSTEM NAME%    API made system 1
    Wait Until Element Is Visible    ${MERGE FORM}//p[contains(@class,"warning-label") and contains(text(),"${error message}")]
    Click Button    ${MERGE CANCEL BUTTON}
    Disconnect from cloud
    Disconnect from cloud

Merge with 3.0
    Log In    ${EMAIL MERGE OWNER 3.0}    ${password}
    Validate Log In
    ${user}    Set Variable    ${EMAIL MERGE OWNER 3.0}
    ${auth}=    Create List    ${user}    ${password}
    Create system and attach to cloud    ${user}    ${image}    7001    API made system 1
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}    180
    Elements Should Not Be Visible    ${MERGE BUTTON SYSTEM}
    Go To    ${url}/systems
    Validate Log In
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}    180
    Wait Until Element Is Enabled    ${MERGE BUTTON SYSTEM}
    Click Button    ${MERGE BUTTON SYSTEM}
    Wait Until Elements Are Visible    ${MERGE FORM}    ${MERGE SYSTEM DROPDOWN}    ${MERGE OK BUTTON}    ${MERGE CANCEL BUTTON}    ${MERGE X BUTTON}
    Wait Until Element Is Visible    ${MERGE SYSTEM DROPDOWN}//span[contains(text()," – incompatible")]
    Click Button    ${MERGE CANCEL BUTTON}
    Disconnect from cloud

2 Systems with identical servers
    ${user}    Set Variable    ${EMAIL MERGE OWNER 2}
    ${auth}=    Create List    ${user}    ${password}
    Create system and attach to cloud    ${user}    ${image}    7001    API made system 1
    Create system and attach to cloud    ${user}    ${image}    7003    API made system 2
    log in    ${user}    ${password}
    Validate Log in
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}    180
    Go To    ${url}/systems
    Validate Log In
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Verify In System    API made system 2
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}    180

    Merge    API made system 1    API made system 1    API made system 1
    Validate Merge

    stop containers    allContainers=False

    Create system and attach to cloud    ${user}    ${image}    7003    API made system 2
    Go To    ${url}/systems
    Wait Until Element Is Visible    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Verify In System    API made system 2
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}    180
    Go To    ${url}/systems
    Wait Until Element Is Visible    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Verify In System    API made system 1
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}    180

    Merge    API made system 1    API made system 2    API made system 2

    Check For Alert    System merge failed
    Wait Until Element Is Visible    ${MERGE FAILED DIALOG HEADER}
    Click Button    ${MERGE FAILED OK BUTTON}
    Element Should Not Be Visible    ${MERGE FAILED DIALOG HEADER}

    Merge    API made system 1    API made system 2    API made system 2

    Check For Alert    System merge failed
    Wait Until Element Is Visible    ${MERGE FAILED DIALOG HEADER}
    Click Button    ${MERGE FAILED X BUTTON}
    Element Should Not Be Visible    ${MERGE FAILED DIALOG HEADER}

    Disconnect from cloud
    ${state}    Run Keyword And Ignore Error    Element Should Be Visible    ${YOU HAVE NO SYSTEMS}
    ${count}    Run Keyword And Ignore Error    Get Element Count    ${SYSTEMS TILE}
    FOR    ${idx}    IN RANGE   ${count}[1]-1
        Click Element    ${SYSTEMS TILE}
        Disconnect from cloud
    END
    Run Keyword Unless    "${state[0]}"=="PASS"    Disconnect from cloud

From secondary system merge to primary with no other systems
    [tags]    C53944
    ${user}    Set Variable    ${EMAIL MERGE OWNER 1}
    ${auth}=    Create List    ${user}    ${password}
    Create system and attach to cloud    ${user}    ${image}    7001    API made system 1    network=host
    Create system and attach to cloud    ${user}    ${image}    7003    API made system 2
    log in    ${user}    ${password}
    Validate Log in
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Validate system available    API made system 1
    Go To    ${url}/systems
    Reload Page
    Validate Log In
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Validate system available    API made system 2

    Merge    API made system 1    API made system 1    API made system 1
    Validate Merge

    Check for alert    Connection to API made system 2 lost    timeout=120
    Wait Until Element Is Visible    ${USERS LIST}
    Wait Until Element Is Not Visible    ${CURRENTLY MERGING CARD}    120
    Validate system available    API made system 1

    Disconnect from cloud

From secondary system merge to primary with other systems
    ${user}    Set Variable    ${EMAIL MERGE OWNER 2}
    ${auth}=    Create List    ${user}    ${password}
    Create system and attach to cloud    ${user}    ${image}    7001    API made system 1
    Create system and attach to cloud    ${user}    ${image}    7003    API made system 2
    Create system and attach to cloud    ${user}    ${image}    7004    API made system 3
    log in    ${user}    ${password}
    Validate Log in
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Validate system available    API made system 1
    Go To    ${url}/systems
    Validate Log In
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Validate system available    API made system 2
    Go To    ${url}/systems
    Validate Log In
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 3")]
    Validate system available    API made system 3

    Merge    API made system 1    API made system 1    API made system 1
    Validate Merge

    Check for alert    Connection to API made system 3 lost    timeout=120
    Wait Until Elements Are Visible    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Validate system available    API made system 1
    Disconnect from cloud
    Disconnect from cloud

From primary system
    [tags]    C48946
    ${user}    Set Variable    ${EMAIL MERGE OWNER 1}
    ${auth}=    Create List    ${user}    ${password}
    Create system and attach to cloud    ${user}    ${image}    7001    API made system 2
    Create system and attach to cloud    ${user}    ${image}    7003    API made system 1
    log in    ${user}    ${password}
    Validate Log in
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Validate system available    API made system 2
    Go To    ${url}/systems
    Reload Page
    Validate Log In
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Validate system available    API made system 1

    Merge    API made system 1    API made system 2    API made system 2
    Validate Merge

    Wait Until Element Is Visible    ${USERS LIST}
    Wait Until Element Is Not Visible    ${CURRENTLY MERGING CARD}    120
    Check For Alert Dismissable    ${SYSTEM MERGE COMPLETED TEXT}
    Validate system available    API made system 1
    Disconnect from cloud

Merge with different types of users
    [tags]    C53946
    ${user}    Set Variable    ${EMAIL MERGE OWNER 2}
    ${auth}=    Create List    ${user}    ${password}
    Prepare System With Users    ${user}    ${auth}    ${image}    7001    API made system 1    ${users dict 1}    network=bridge
    Prepare System With Users    ${user}    ${auth}    ${image}    7003    API made system 2    ${users dict 2}    network=bridge
    Log In    ${user}    ${password}
    Validate Log in
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 1")]
    Validate system available    API made system 1
    Go To    ${url}/systems
    Validate Log In
    Click Element    ${SYSTEMS TILE}//h2[contains(text(),"API made system 2")]
    Validate system available    API made system 2

    Merge    API made system 1    API made system 1    API made system 1
    Validate Merge

    Check for alert    Connection to API made system 2 lost    timeout=120
    Validate system available    API made system 1
    FOR     ${idx}  IN RANGE  90
        ${result}    Run Keyword And Ignore Error    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${email admin no reg}')]
        Reload Page
        Exit For Loop If    '${result[0]}'=='PASS'
    END
    FOR    ${key}  IN  @{all users dict.keys()}
        Check User Permissions    ${all users dict["${key}"]}    ${key}    timeout=120
    END
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${email admin no reg}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${email admin no reg}')]/following-sibling::td/a[@ng-click='unshare(user)']/span[contains(text(),'${DELETE USER BUTTON TEXT}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${email admin no reg}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'${EDIT USER BUTTON TEXT}')]/..
    Disconnect from cloud