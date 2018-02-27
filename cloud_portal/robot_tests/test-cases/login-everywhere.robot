*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${email}    ${EMAIL OWNER}
${email invalid}    aodehurgjaegir
${password}    ${BASE PASSWORD}
${url}         ${CLOUD TEST}

*** Keywords ***
Check Log In
    Log In    ${EMAIL UNREGISTERED}    ${password}
    Check For Alert    ${ACCOUNT DOES NOT EXIST}
    Log In    ${email}    ${password}    None
    Validate Log In

*** Test Cases ***
works at registration page before submit
    Open Browser and go to URL    ${url}/register
    Check Log In
    Close Browser

works at registration page after submit success
    Open Browser and go to URL    ${url}/register
    ${random email}    Get Random Email
    Register    mark    hamill    ${random email}    ${password}
    Validate Register Success
    Check Log In
    Close Browser

works at registration page after submit with alert error message
    Open Browser and go to URL    ${url}/register
    ${random email}    Get Random Email
    Register    mark    hamill    ${email}    ${password}
    Wait Until Element Is Visible    ${EMAIL ALREADY REGISTERED}
    Check Log In
    Close Browser

works at registration page on account activation success
    [tags]    email
    Open Browser and go to URL    ${url}/register
    ${random email}    Get Random Email
    Register    mark    hamill    ${random email}    ${password}
    Activate    ${random email}
    Check Log In
    Close Browser

works at registration page on account activation error
    [tags]    email
    ${random email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    'mark'    'hamill'    ${random email}    ${password}
    ${link}    Get Activation Link    ${random email}
    Go To    ${link}
    Wait Until Element Is Visible    ${ACTIVATION SUCCESS}
    Go To    ${link}
    Wait Until Element Is Visible    ${ALREADY ACTIVATED}
    Check Log In
    Close Browser

works at restore password page with email input - before submit
    Open Browser and go to URL    ${url}/restore_password
    Check Log In
    Close Browser

works at restore password page with email input - after submit error
    Open Browser and go to URL    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${EMAIL UNREGISTERED}
    Click Button    ${RESET PASSWORD BUTTON}
    Check For Alert Dismissable    ${CANNOT SEND CONFIRMATION EMAIL} ${ACCOUNT DOES NOT EXIST}
    Check Log In
    Close Browser

works at restore password page with email input - after submit success
    Open Browser and go to URL    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    Wait Until Element Is Visible    ${RESET EMAIL SENT MESSAGE}
    Check Log In
    Close Browser

works at restore password page with password input - before submit
    [tags]    email
    ${random email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${random email}    ${password}
    ${link}    Get Activation Link    ${random email}
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${random email}
    Click Button    ${RESET PASSWORD BUTTON}
    Wait Until Element Is Visible    ${RESET EMAIL SENT MESSAGE}
    ${link}    Get Reset Password Link    ${random email}
    Go To    ${link}
    Check Log In
    Close Browser

works at restore password page with password input - after submit error
    [tags]    email
    ${random email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${random email}    ${password}
    ${link}    Get Activation Link    ${random email}
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${random email}
    Click Button    ${RESET PASSWORD BUTTON}
    Wait Until Element Is Visible    ${RESET EMAIL SENT MESSAGE}
    ${link}    Get Reset Password Link    ${random email}
    Go To    ${link}
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${EMPTY}
    Click Button    ${SAVE PASSWORD}
    Wait Until Element Is Visible    ${PASSWORD IS REQUIRED}
    Close Browser

works at restore password page with password input - after submit success
    [tags]    email
    ${random email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${random email}    ${password}
    ${link}    Get Activation Link    ${random email}
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${random email}
    Click Button    ${RESET PASSWORD BUTTON}
    Wait Until Element Is Visible    ${RESET EMAIL SENT MESSAGE}
    ${link}    Get Reset Password Link    ${random email}
    Go To    ${link}
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${password}
    Click Button    ${SAVE PASSWORD}
    Wait Until Elements Are Visible    ${RESET SUCCESS MESSAGE}    ${RESET SUCCESS LOG IN LINK}
    Check Log In
    Close Browser