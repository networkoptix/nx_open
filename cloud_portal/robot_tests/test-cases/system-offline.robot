*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers
Force Tags        system

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${ENV}

*** Keywords ***
Log in to Autotests System
    [arguments]    ${email}
    Go To    ${url}/systems/${AUTOTESTS OFFLINE SYSTEM ID}
    Log In    ${email}    ${password}    None
    Validate Log In
    Run Keyword If    '${email}' == '${EMAIL OWNER}'    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword If    '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword Unless    '${email}' == '${EMAIL OWNER}' or '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${OPEN IN NX BUTTON}

*** Test Cases ***
should confirm, if owner deletes system (You are going to disconnect your system from cloud)
    Open Browser and go to URL    ${url}
    Log in to Autotests System    ${EMAIL OWNER}
    Click Button    ${DISCONNECT FROM NX}
    Wait Until Elements Are Visible    ${DISCONNECT FORM}    ${DISCONNECT FORM HEADER}
    Close Browser

should confirm, if not owner deletes system (You will loose access to this system)
    Open Browser and go to URL    ${url}
    Log in to Autotests System    ${EMAIL OWNER}
    Validate Log In
    Wait Until Element Is Visible    ${DISCONNECT FROM NX}
    Click Button    ${DISCONNECT FROM NX}
    Wait Until Elements Are Visible    ${DISCONNECT FORM}    ${DISCONNECT FORM HEADER}
    Close Browser

share button should be disabled
    Open Browser and go to URL    ${url}
    Log in to Autotests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${SHARE BUTTON DISABLED}
    Close Browser

open in nx button should be disabled
    Open Browser and go to URL    ${url}
    Log in to Autotests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${OPEN IN NX BUTTON DISABLED}
    Close Browser

should show offline next to system name
    Open Browser and go to URL    ${url}
    Log in to Autotests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${SYSTEM NAME OFFLINE}
    Close Browser

should not be able to delete/edit users
    Open Browser and go to URL    ${url}
    Log in to Autotests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL VIEWER}')]
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL VIEWER}')]
#This sleep is to make sure that I am not looking for the element before it loads.  That could give a false positive.
    Sleep    3
    Wait Until Element Is Not Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL VIEWER}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'Edit')]/..
    Wait Until Element Is Not Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL VIEWER}')]/following-sibling::td/a[@ng-click='unshare(user)']/span['&nbsp&nbspDelete']
    Close Browser

should open System page by link to not authorized user and redirect to homepage, if he does not log in
    Open Browser and go to URL    ${url}/systems/${AUTOTESTS OFFLINE SYSTEM ID}
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Click Button    ${LOG IN CLOSE BUTTON}
    Wait Until Element Is Visible    ${JUMBOTRON}
    Close Browser

should open System page by link to not authorized user and show it, after owner logs in
    Open Browser and go to URL    ${url}/systems/${AUTOTESTS OFFLINE SYSTEM ID}
    Log In    ${EMAIL OWNER}   ${password}    None
    Verify In System    Auto Tests 2
    Close Browser

should open System page by link to user without permission and show alert (System info is unavailable: You have no access to this system)
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTOTESTS OFFLINE SYSTEM ID}
    Wait Until Element Is Visible    ${SYSTEM NO ACCESS}
    Close Browser

should open System page by link not authorized user, and show alert if logs in and has no permission
    Open Browser and go to URL    ${url}/systems/${AUTOTESTS OFFLINE SYSTEM ID}
    Log In    ${EMAIL NOPERM}   ${password}    None
    Wait Until Element Is Visible    ${SYSTEM NO ACCESS}
    Close Browser

rename button opens dialog; cancel closes without rename; save renames system when offline
    Open Browser and go to URL    ${url}
    Log in to Autotests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}
    Click Button    ${RENAME CANCEL}
    Wait Until Page Does Not Contain Element    //div[@modal-render='true']
    Verify In System    Auto Tests 2
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}    ${RENAME INPUT}
    Clear Element Text    ${RENAME INPUT}
    Input Text    ${RENAME INPUT}    Auto Tests Rename
    Click Button    ${RENAME SAVE}
    Check For Alert    ${SYSTEM NAME SAVED}
    Verify In System    Auto Tests Rename
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}    ${RENAME INPUT}
    Clear Element Text    ${RENAME INPUT}
    Input Text    ${RENAME INPUT}    Auto Tests 2
    Click Button    ${RENAME SAVE}
    Check For Alert    ${SYSTEM NAME SAVED}
    Verify In System    Auto Tests 2
    Close Browser

#This can't really be tested unless I can add to an offline system
does not show Share button to viewer, advanced viewer, live viewer
    [tags]    not-ready
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${EMAIL VIEWER}
    Register Keyword To Run On Failure    NONE
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Register Keyword To Run On Failure    Failure Tasks
    Element Should Not Be Visible    ${SHARE BUTTON SYSTEMS}
    Log Out
    Log in to Auto Tests System    ${EMAIL ADV VIEWER}
    Register Keyword To Run On Failure    NONE
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Register Keyword To Run On Failure    Failure Tasks
    Element Should Not Be Visible    ${SHARE BUTTON SYSTEMS}
    Log Out
    Log in to Auto Tests System    ${EMAIL LIVE VIEWER}
    Register Keyword To Run On Failure    NONE
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Register Keyword To Run On Failure    Failure Tasks
    Element Should Not Be Visible    ${SHARE BUTTON SYSTEMS}
    Close Browser