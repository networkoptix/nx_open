#This script is for adding QA users to a new db
*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${url}         ${ENV}
${password}    ${BASE PASSWORD}
@{emails}    ${EMAIL VIEWER}    ${EMAIL ADV VIEWER}    ${EMAIL LIVE VIEWER}    ${EMAIL OWNER}    ${EMAIL NOT OWNER}   ${EMAIL ADMIN}    ${EMAIL NOPERM}    ${EMAIL CUSTOM}

*** Keywords ***
register and activate
    [arguments]    ${email}
    register    mark    hamil    ${email}    ${password}
    activate    ${email}


*** Test Cases ***
create Owner, Admin, Adv Viewer, Viewer, Live Viewer, noperm, notowner
    Open Browser and go to URL    ${url}
    :FOR    ${user}    IN    @{emails}
    \  Set Selenium Speed    .25
    \  Register Keyword To Run On Failure    Capture Page Screenshot
    \  Log In    ${user}    ${password}
    \  Register Keyword To Run On Failure    NONE
    \  ${present}    Run Keyword And Return Status    Element Should Be Visible    ${ALERT}
    \  ${message}    Run Keyword If    ${present}    Get Text    ${ALERT}
    \  Register Keyword To Run On Failure    Capture Page Screenshot
    \  Run Keyword If    "${message}"=="${ACCOUNT DOES NOT EXIST}"    Press Key   ${EMAIL INPUT}    ${ESCAPE}
    \  Run Keyword If    "${message}"=="${ACCOUNT DOES NOT EXIST}"    Go To    ${url}/register
    \  Run Keyword If    "${message}"=="${ACCOUNT DOES NOT EXIST}"    register and activate    ${user}
    \  Run Keyword Unless    "${message}"=="${ACCOUNT DOES NOT EXIST}"    Validate Log In
    \  Run Keyword Unless    "${message}"=="${ACCOUNT DOES NOT EXIST}"    Log Out