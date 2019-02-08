*** Settings ***
Resource          ../resource.robot
Test Setup        Restart
#Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers
Force Tags        Threaded

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
    Wait Until Element Is Visible    ${RELEASE NUMBER}
    ${first number}    Get Text    ${RELEASE NUMBER}
    #create an element to reference that will always refer to elements in the first section of each tab
    ${first section}    Set Variable If    ${FULL}==False    //div[contains(@class,"active")]//h1[contains(text(),'${first number}')]
    #get all or just first section
    ${expandables}    Run Keyword If    ${FULL}==True    Get WebElements    //div[contains(@class,"active")]//div/a
    ...    ELSE    Get WebElements    ${first section}/../..//div/a
    Run Keyword Unless    ${expandables}    Fail    Expandables was empty
    #open the expanders
    : FOR    ${platform}    IN    @{expandables}
    \    Click Link    ${platform}
    \    ${downloads}=    Run Keyword If    ${FULL}==True    Get WebElements    //div[contains(@class,"active")]//div/a/../ul/li/a
    \    ...    ELSE    Get WebElements    ${first section}/../..//div/ul/li/a
    \    loop links    ${downloads}

#check each link in each expander for validity
loop links
    [arguments]    ${downloads}
    : FOR    ${download}    IN    @{downloads}
    \    ${link}    Get Element Attribute    ${download}    href
    \    ${matches}    Get Regexp Matches    ${link}    ${DOWNLOADS DOMAIN}
    \    Run Keyword If    ${matches}    Check File Exists    ${link}
    ...    ELSE    Fail    URL did not begin with ${DOWNLOADS DOMAIN}

*** Test Cases ***
History link is not in the downloads page for user without access
    Log In    ${EMAIL VIEWER}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${DOWNLOAD LINK}
    Click Link    ${DOWNLOAD LINK}
    Register Keyword To Run On Failure    NONE
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${RELEASE HISTORY BUTTON}

History link is in the downloads page for user with access and takes you to /downloads/history
    Log In    ${email}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${DOWNLOAD LINK}
    Click Link    ${DOWNLOAD LINK}
    Wait Until Elements Are Visible    ${DOWNLOADS HEADER}    ${WINDOWS TAB}
    Click Link    ${WINDOWS TAB}
    Wait Until Elements Are Visible    ${DOWNLOAD WINDOWS VMS LINK}    ${RELEASE HISTORY BUTTON}
    Click Link    ${RELEASE HISTORY BUTTON}
    Location Should Be    ${url}/downloads/history

Going to the history page anonymous asks for login and closing takes you back to home
    Go To    ${url}/downloads/history
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Click Button    ${LOG IN CLOSE BUTTON}
    Location Should Be    ${url}/

Going to the history page anonymous asks for login and login shows history page
    Go To    ${url}/downloads/history
    Log In    ${email}   ${password}    button=None
    Validate Log In
    Wait Until Element Is Visible    ${RELEASES TAB}
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

    Wait Until Element Is Visible    ${PATCHES TAB}
    loop expanders
    Click Link    ${PATCHES TAB}
    Wait Until Element Is Visible    ${PATCHES TAB}
    loop expanders
    Click Link    ${BETAS TAB}
    Wait Until Element Is Visible    ${PATCHES TAB}
    loop expanders