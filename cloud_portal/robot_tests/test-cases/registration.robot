*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${password}    qweasd123
${url}         ${CLOUD TEST}

*** Test Cases ***
Register and Activate
    ${email}    Get Random Email
    Set Global Variable    ${random email}    ${email}
    Open Browser and go to URL    ${url}/register
    Register    'mark'    'hamill'    ${random email}    ${password}
    Get Activation Link    ${random email}
    Wait Until Element Is Visible    ${ACTIVATION SUCCESS}
    Element Should Be Visible    ${ACTIVATION SUCCESS}
    ${location}    Get Location
    Should Be Equal    ${location}    ${url}/activate/success
    Wait Until Element Is Visible    ${SUCCESS LOG IN BUTTON}
    Click Link    ${SUCCESS LOG IN BUTTON}
    Log In    ${email}    ${password}
    Validate Log In
    Close Browser