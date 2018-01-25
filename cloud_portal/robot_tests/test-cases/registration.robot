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
    Open Browser and go to URL    ${url}/register
    Register    'mark'    'hamill'    ${email}    ${password}
    ${link}    Get Activation Link    ${email}
    Go To    ${link}
    Wait Until Element Is Visible    ${ACTIVATION SUCCESS}
    Element Should Be Visible    ${ACTIVATION SUCCESS}
    ${location}    Get Location
    Should Be Equal    ${location}    ${url}/activate/success
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In
    Close Browser

should show error if same link is used twice
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    'mark'    'hamill'    ${email}    ${password}
    ${link}    Get Activation Link    ${email}
    Go To    ${link}
    Wait Until Element Is Visible    ${ACTIVATION SUCCESS}
    Element Should Be Visible    ${ACTIVATION SUCCESS}
    Go To    ${link}
    Wait Until Element Is Visible    ${ALREADY ACTIVATED}
    Element Should Be Visible    ${ALREADY ACTIVATED}
    Close Browser