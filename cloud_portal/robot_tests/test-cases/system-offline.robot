*** Settings ***
Resource          ../resource.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers
Force Tags        system

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${ENV}

*** Keywords ***
Log in to Autotests 2 System
    [arguments]    ${email}
    Go To    ${url}/systems/${AUTOTESTS OFFLINE SYSTEM ID}
    Log In    ${email}    ${password}    None
    Validate Log In
    Run Keyword If    '${email}' == '${EMAIL OWNER}'    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword If    '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword Unless    '${email}' == '${EMAIL OWNER}' or '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${OPEN IN NX BUTTON}

Restart
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}

Open New Browser On Failure
    Close Browser
    Reset System Names
    Open Browser and go to URL    ${url}

*** Test Cases ***
the page is opened and shows the user list to owner
    [tags]    C41881    Threaded
    Log in to Autotests 2 System    ${EMAIL OWNER}
    Location Should Be    ${url}/systems/${AUTOTESTS OFFLINE SYSTEM ID}
    Wait Until Element Is Visible    ${USERS LIST}

should confirm, if owner deletes system (You are going to disconnect your system from cloud)
    [tags]    Threaded
    Log in to Autotests 2 System    ${EMAIL OWNER}
    Click Button    ${DISCONNECT FROM NX}
    Wait Until Elements Are Visible    ${DISCONNECT FORM}    ${DISCONNECT FORM HEADER}    ${DISCONNECT FORM CANCEL}
    Click Button    ${DISCONNECT FORM CANCEL}
    Wait Until Page Does Not Contain Element    ${BACKDROP}

should confirm, if not owner deletes system (You will loose access to this system)
    [tags]    Threaded
    Log in to Autotests 2 System    ${EMAIL OWNER}
    Validate Log In
    Wait Until Element Is Visible    ${DISCONNECT FROM NX}
    Click Button    ${DISCONNECT FROM NX}
    Wait Until Elements Are Visible    ${DISCONNECT FORM}    ${DISCONNECT FORM HEADER}    ${DISCONNECT FORM CANCEL}
    Click Button    ${DISCONNECT FORM CANCEL}
    Wait Until Page Does Not Contain Element    ${DELETE USER MODAL}

share button should be disabled
    [tags]    C41881    Threaded
    Set Window Size    1920    1080
    Log in to Autotests 2 System    ${EMAIL OWNER}
    Wait Until Page Does Not Contain Element    //div[contains(@uib-modal-backdrop, "modal-backdrop")]
    Wait Until Element Is Visible    ${SHARE BUTTON DISABLED}

open in nx button should be disabled
    [tags]    C41881    Threaded
    Log in to Autotests 2 System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${OPEN IN NX BUTTON DISABLED}
    Log Out
    Validate Log Out
    Log in to Autotests 2 System    ${EMAIL VIEWER}
    Wait Until Element Is Visible    ${OPEN IN NX BUTTON DISABLED}

should show offline next to system name
    [tags]    C41881    Threaded
    Log in to Autotests 2 System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${SYSTEM NAME OFFLINE}
    Log Out
    Validate Log Out
    Log in to Autotests 2 System    ${EMAIL VIEWER}
    Wait Until Element Is Visible    ${SYSTEM NAME OFFLINE}

should not be able to delete/edit users
    [tags]    Threaded
    Log in to Autotests 2 System    ${EMAIL OWNER}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL VIEWER}')]
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL VIEWER}')]
#This sleep is to make sure that I am not looking for the element before it loads.  That could give a false positive.
    Sleep    3
    Wait Until Element Is Not Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL VIEWER}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'Edit')]/..
    Wait Until Element Is Not Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL VIEWER}')]/following-sibling::td/a[@ng-click='unshare(user)']/span['&nbsp&nbspDelete']

should open System page by link to not authorized user and redirect to homepage, if he does not log in
    [tags]    Threaded
    Go To    ${url}/systems/${AUTOTESTS OFFLINE SYSTEM ID}
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Click Button    ${LOG IN CLOSE BUTTON}
    Wait Until Element Is Visible    ${JUMBOTRON}

should open System page by link to not authorized user and show it, after owner logs in
    [tags]    Threaded
    Go To    ${url}/systems/${AUTOTESTS OFFLINE SYSTEM ID}
    Log In    ${EMAIL OWNER}   ${password}    None
    Verify In System    Auto Tests 2

should open System page by link to user without permission and show alert (System info is unavailable: You have no access to this system)
    [tags]    C41572    Threaded
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTOTESTS OFFLINE SYSTEM ID}
    Wait Until Elements Are Visible    ${SYSTEM NO ACCESS}    ${AVAILABLE SYSTEMS LIST}
    Click Link    ${AVAILABLE SYSTEMS LIST}
    Location Should Be    ${url}/systems

should open System page by link not authorized user, and show alert if logs in and has no permission
    [tags]    Threaded
    Go To    ${url}/systems/${AUTOTESTS OFFLINE SYSTEM ID}
    Log In    ${EMAIL NOPERM}   ${password}    None
    Wait Until Element Is Visible    ${SYSTEM NO ACCESS}

rename button opens dialog and clicking cancel closes rename dialog without rename
    [tags]    C41880    Threaded
    Log in to Autotests 2 System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}
    Click Button    ${RENAME CANCEL}
    Wait Until Page Does Not Contain Element    //div[@uib-modal-backdrop="modal-backdrop"]
    Verify In System    Auto Tests 2

clicking 'X' closes rename dialog without rename
    [tags]    C41880    Threaded
    Log in to Autotests 2 System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}    ${RENAME X BUTTON}
    Wait Until Textfield Contains    ${RENAME INPUT}    ${AUTO TESTS 2}
    Click Button    ${RENAME X BUTTON}
    Wait Until Page Does Not Contain Element    ${BACKDROP}
    Verify In System    Auto Tests 2

clicking save with no input in rename dialoge throws error
    [tags]    C41880    Threaded
    Log in to Autotests 2 System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME SYSTEM}    ${OPEN IN NX BUTTON}    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}    ${RENAME INPUT}
    sleep    2
    Input Text    ${RENAME INPUT}    ${SPACE}
    Press Key    ${RENAME INPUT}    ${BACKSPACE}
    Click Button    ${RENAME SAVE}
    Wait Until Elements Are Visible    ${RENAME INPUT WITH ERROR}    ${SYSTEM NAME IS REQUIRED}
    Click Button    ${RENAME CANCEL}

does not show Share button to viewer, advanced viewer, live viewer
    [tags]    Threaded
    @{emails}    Set Variable    ${EMAIL VIEWER}    ${EMAIL LIVE VIEWER}    ${EMAIL ADV VIEWER}
    :FOR    ${user}    IN    @{emails}
    \  Log in to Autotests 2 System    ${user}
    \  Register Keyword To Run On Failure    NONE
    \  Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    \  Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${RENAME SYSTEM}
    \  Register Keyword To Run On Failure    Failure Tasks
    \  Element Should Not Be Visible    ${SHARE BUTTON SYSTEMS}
    \  Log Out

should show "your system" for owner and "owner's name" for non-owners
    [tags]    C41881    Threaded
    Log in to AutoTests 2 System    ${EMAIL OWNER}
    Wait Until Element Is Visible    //h2[.='${YOUR SYSTEM TEXT}']
    Wait Until Element Is Not Visible    //h2[.='${OWNER TEXT}']
    Log Out
    Validate Log Out
    Log in to Autotests 2 System    ${EMAIL VIEWER}
    Wait Until Elements Are Visible    //h2[.='${OWNER TEXT}']    //a[.='${EMAIL OWNER}']
    Wait Until Element Is Not Visible    //h2[.='${YOUR SYSTEM TEXT}']