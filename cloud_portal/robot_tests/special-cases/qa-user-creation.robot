*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${url}         ${ENV}
${password}    ${BASE PASSWORD}
@{emails}    noptixqa+viewer@gmail.com    noptixqa+advviewer@gmail.com    noptixqa+liveviewer@gmail.com     noptixqa+owner@gmail.com    noptixqa+notowner@gmail.com    noptixqa+admin@gmail.com    noptixqa+noperm@gmail.com

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
    \  Run Keyword If    "${message}"=="Account does not exist"    Press Key   ${EMAIL INPUT}    ${ESCAPE}
    \  Run Keyword If    "${message}"=="Account does not exist"    Go To    ${url}/register
    \  Run Keyword If    "${message}"=="Account does not exist"    register and activate    ${user}
    \  Run Keyword Unless    "${message}"=="Account does not exist"    Validate Log In
    \  Run Keyword Unless    "${message}"=="Account does not exist"    Log Out