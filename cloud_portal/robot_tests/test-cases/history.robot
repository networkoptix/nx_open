*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Setup        Restart
#Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers

*** Variables ***
${email}           ${EMAIL OWNER}
${password}        ${BASE PASSWORD}
${url}             ${ENV}
${FULL}            False


*** Keywords ***
Restart
    Go To    ${url}
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}
    Validate Log Out

Open New Browser On Failure
    Close Browser
    Open Browser and go to URL    ${url}

Log in to downloads/history
    Go To    ${url}/downloads/history
    Log In    ${email}    ${password}    button=None
    Validate Log In

loop expanders
    #get the first release number for targeting purposes
    ${first number}    Get Text    ${RELEASE NUMBER}
    #create an element to reference that will always refer to elements in the first section of each tab
    ${first section}    Set Variable If    ${FULL}==False    //div[contains(@class,"active")]//div[@ng-repeat="release in activeBuilds"]//h1/b[text()='${first number}']
    #get all or just first section
    ${expandables}    Run Keyword If    ${FULL}==True    Get WebElements    //div[contains(@class,"active")]//div[@ng-repeat="platform in release.platforms"]/h5/a[@ng-click="expand[platform.name] = !expand[platform.name]"]
    ...    ELSE    Get WebElements    ${first section}/../..//div[@ng-repeat="platform in release.platforms"]/h5/a[@ng-click="expand[platform.name] = !expand[platform.name]"]
    #open the expanders
    : FOR    ${platform}    IN    @{expandables}
    \    Click Link    ${platform}
    \    ${downloads}=    Run Keyword If    ${FULL}==True    Get WebElements    //div[contains(@class,"active")]//div[@ng-repeat="platform in release.platforms"]/h5/a[@ng-click="expand[platform.name] = !expand[platform.name]"]/../../ul/li/a
    \    ...    ELSE    Get WebElements    ${first section}/../..//div[@ng-repeat="platform in release.platforms"]/h5/a[@ng-click="expand[platform.name] = !expand[platform.name]"]/../../ul/li/a
    \    loop links    ${downloads}

#check each link in each expander for validity
loop links
    [arguments]    ${downloads}
    : FOR    ${download}    IN    @{downloads}
    \    ${link}    Get Element Attribute    ${download}    href
    \    Check File Exists    ${link}

*** Test Cases ***
History link is not in the account dropdown for user without access
    Log In    ${EMAIL VIEWER}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Click Element    ${ACCOUNT DROPDOWN}
    Register Keyword To Run On Failure    NONE
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${RELEASE HISTORY BUTTON}

History link is in the account dropdown for user with access and takes you to /downloads/history
    Log In    ${email}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Click Element    ${ACCOUNT DROPDOWN}
    Register Keyword To Run On Failure    NONE
    Wait Until Element Is Visible    ${RELEASE HISTORY BUTTON}
    Click Link    ${RELEASE HISTORY BUTTON}
    Location Should Be    ${url}/downloads/history

Going to the history page anonymous asks for login and closing takes you back to home
    Go To    ${url}/downloads/history
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Click Button    ${LOG IN CLOSE BUTTON}
    Location Should Be    ${url}/

Going to the history page anonymous asks for login and login shows downloads page
    Go To    ${url}/downloads/history
    Log In    ${email}   ${password}    button=None
    Validate Log In
    Location Should Be    ${url}/downloads/history

Going to the history page anonymous and logging in with someone who doesn't have access takes you to 404
    Go To    ${url}/downloads/history
    Log In    ${EMAIL VIEWER}   ${password}    button=None
    Wait Until Elements Are Visible    ${PAGE NOT FOUND}    ${TAKE ME HOME}
    Location Should Be    ${url}/404

Going to the history page while logged in as someone who doesn't have access takes you to 404
    Log In    ${EMAIL VIEWER}    ${password}
    Validate Log In
    Go To    ${url}/downloads/history
    Wait Until Elements Are Visible    ${PAGE NOT FOUND}    ${TAKE ME HOME}
    Location Should Be    ${url}/404

#Make sure each tab changes to a unique release number
Make sure expandable sections show options
    Log in to downloads/history
    sleep     5
    loop expanders
    Click Link    ${PATCHES TAB}
    loop expanders
    Click Link    ${BETAS TAB}
    loop expanders