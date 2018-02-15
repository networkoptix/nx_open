*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${CLOUD TEST}

*** Keywords ***
Log in to Autotests System
    [arguments]    ${email}
    Go To    ${url}/systems/${AUTOTESTS SYSTEM ID}
    Log In    ${email}    ${password}    None
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

should not be able to delete/edit users
    Open Browser and go to URL    ${url}
    Log in to Autotests System    ${EMAIL OWNER}