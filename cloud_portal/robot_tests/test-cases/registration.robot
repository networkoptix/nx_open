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

should save user data to user account correctly
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
    Go To    ${url}/account
    Wait Until Element Is Visible    ${ACCOUNT FIRST NAME}
    Textfield Should Contain    ${ACCOUNT FIRST NAME}    mark
    Wait Until Element Is Visible    ${ACCOUNT LAST NAME}
    Textfield Should Contain    ${ACCOUNT LAST NAME}    hamill

should allow to enter more than 256 symbols in First and Last names and cut it to 256
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    ${300CHARS}    ${300CHARS}    ${email}    ${password}
    ${link}    Get Activation Link    ${email}
    Go To    ${link}
    Wait Until Element Is Visible    ${ACTIVATION SUCCESS}
    Element Should Be Visible    ${ACTIVATION SUCCESS}
    ${location}    Get Location
    Should Be Equal    ${location}    ${url}/activate/success
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In
    Go To    ${url}/account
    Wait Until Element Is Visible    ${ACCOUNT FIRST NAME}
    Textfield Should Contain    ${ACCOUNT FIRST NAME}    ${255CHARS}
    Wait Until Element Is Visible    ${ACCOUNT LAST NAME}
    Textfield Should Contain    ${ACCOUNT LAST NAME}    ${255CHARS}
    Close Browser

should trim leading and trailing spaces
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    ${SPACE}mark${SPACE}    ${SPACE}hamill${SPACE}    ${email}    ${password}
    ${link}    Get Activation Link    ${email}
    Go To    ${link}
    Wait Until Element Is Visible    ${ACTIVATION SUCCESS}
    Element Should Be Visible    ${ACTIVATION SUCCESS}
    ${location}    Get Location
    Should Be Equal    ${location}    ${url}/activate/success
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In
    Go To    ${url}/account
    Wait Until Element Is Visible    ${ACCOUNT FIRST NAME}
    Textfield Value Should Be    ${ACCOUNT FIRST NAME}    mark
    Wait Until Element Is Visible    ${ACCOUNT LAST NAME}
    Textfield Value Should Be    ${ACCOUNT LAST NAME}    hamill
    Close Browser