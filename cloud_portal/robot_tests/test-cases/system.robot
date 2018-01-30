*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${CLOUD TEST}

*** Keywords ***
Log in to Auto Tests System
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL}    ${password}
    Validate Log In
    Select Auto Tests System

*** Test Cases ***
has system name, owner and OpenInNx button visible on systems page
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Wait Until Element Is Visible    ${AUTO TESTS TITLE}
    Element Text Should Be    ${AUTO TESTS TITLE}    Auto Tests
    Wait Until Element Is Visible    ${AUTO TESTS USER}
    Element Should Be Visible    ${AUTO TESTS USER}
    Wait Until Element Is Visible    ${AUTO TESTS OPEN NX}
    Element Should Be Visible    ${AUTO TESTS OPEN NX}
    Close Browser

shows offline status and does not show open in nx button when offline
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Wait Until Element Is Visible    ${AUTOTESTS OFFLINE}
    Element Should Be Visible    ${AUTOTESTS OFFLINE}
    Wait Until Element Is Not Visible    ${AUTOTESTS OPEN NX OFFLINE}
    Element Should Not Be Visible    ${AUTOTESTS OPEN NX OFFLINE}
    Close Browser

should confirm, if owner deletes system (You are going to disconnect your system from cloud)
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Select Auto Tests System
    Wait Until Element Is Visible    ${DISCONNECT FROM NX}
    Click Button    ${DISCONNECT FROM NX}
    Wait Until Element Is Visible    ${DISCONNECT FORM}
    Element Should Be Visible    ${DISCONNECT FORM}
    Wait Until Element Is Visible    ${DISCONNECT FORM HEADER}
    Element Should Be Visible    ${DISCONNECT FORM HEADER}
    Close Browser