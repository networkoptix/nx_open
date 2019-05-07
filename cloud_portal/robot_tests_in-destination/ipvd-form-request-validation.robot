*** Settings ***
Resource          resource.robot
Suite Setup       Open Browser and go to URL    ${url}
Test Teardown     Run Keyword If Test Failed    Restart
Test Template     Test Submit Message
Suite Teardown    Close Browser
Force Tags        form    Threaded File

*** Variables ***
${url}         ${ENV}
${password}    ${BASE PASSWORD}

*** Keywords ***
Check advaced search filters text
    [Arguments]    ${fiters}


Reset DB and Open New Browser On Failure
    Close Browser
    Open Browser and go to URL    ${url}

Restart
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}

*** Test Cases ***
IPVD page loads without login