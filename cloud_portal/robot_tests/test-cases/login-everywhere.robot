*** Settings ***
Resource          ../resource.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers

*** Variables ***
${email}    ${EMAIL OWNER}
${email invalid}    aodehurgjaegir
${password}    ${BASE PASSWORD}
${url}         ${ENV}

*** Keywords ***
Check Log In
    ${random email}    Get Random Email    ${BASE EMAIL}
    Log In    ${random email}    ${password}
    Wait Until Element Is Visible    ${ACCOUNT NOT FOUND}
    Log In    ${email}    ${password}    None
    Validate Log In

Open New Browser On Failure
    Close Browser
    Open Browser and go to URL    ${url}

Restart
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}

*** Test Cases ***
works at registration page before submit
    Go To   ${url}/register
    Check Log In

works at registration page after submit success
    Go To    ${url}/register
    ${random email}    Get Random Email    ${BASE EMAIL}
    Register    mark    hamill    ${random email}    ${password}
    Validate Register Success
    Check Log In

works at registration page after submit with alert error message
    Go To    ${url}/register
    ${random email}    Get Random Email    ${BASE EMAIL}
    Register    mark    hamill    ${email}    ${password}
    Wait Until Element Is Visible    ${EMAIL ALREADY REGISTERED}
    Check Log In

works at registration page on account activation success
    [tags]    email
    Go To    ${url}/register
    ${random email}    Get Random Email    ${BASE EMAIL}
    Register    mark    hamill    ${random email}    ${password}
    Activate    ${random email}
    Check Log In

works at registration page on account activation error
    [tags]    email
    ${random email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    'mark'    'hamill'    ${random email}    ${password}
    ${link}    Get Email Link    ${random email}    activate
    Go To    ${link}
    Wait Until Element Is Visible    ${ACTIVATION SUCCESS}
    Go To    ${link}
    Wait Until Element Is Visible    ${ALREADY ACTIVATED}
    Check Log In

works at restore password page with email input - before submit
    Go To    ${url}/restore_password
    Check Log In

works at restore password page with email input - after submit error
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${EMAIL UNREGISTERED}
    Click Button    ${RESET PASSWORD BUTTON}
    Check For Alert Dismissable    ${CANNOT SEND CONFIRMATION EMAIL}${SPACE}${ACCOUNT DOES NOT EXIST}
    Check Log In

works at restore password page with email input - after submit success
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${RESET EMAIL SENT MESSAGE TEXT}    Replace String    ${RESET EMAIL SENT MESSAGE TEXT}    %email%    ${email}
    Wait Until Element Is Visible    ${RESET EMAIL SENT MESSAGE}
    ${text}    Get Text    ${RESET EMAIL SENT MESSAGE}
    ${replaced}    Replace String    ${text}    \n    ${SPACE}
    Should Match    ${replaced}    ${RESET EMAIL SENT MESSAGE TEXT}
    Check Log In

works at restore password page with password input - before submit
    [tags]    email
    ${random email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${random email}    ${password}
    ${link}    Get Email Link    ${random email}    activate
    ${link}    Strip String    ${link}
    Go To    ${link}
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${random email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${RESET EMAIL SENT MESSAGE TEXT}    Replace String    ${RESET EMAIL SENT MESSAGE TEXT}    %email%    ${random email}
    Wait Until Element Is Visible    ${RESET EMAIL SENT MESSAGE}
    ${text}    Get Text    ${RESET EMAIL SENT MESSAGE}
    ${replaced}    Replace String    ${text}    \n    ${SPACE}
    Should Match    ${replaced}    ${RESET EMAIL SENT MESSAGE TEXT}
    ${link}    Get Email Link    ${random email}    restore_password
    Go To    ${link}
    Check Log In

works at restore password page with password input - after submit error
    [tags]    email
    ${random email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${random email}    ${password}
    ${link}    Get Email Link    ${random email}    activate
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${random email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${RESET EMAIL SENT MESSAGE TEXT}    Replace String    ${RESET EMAIL SENT MESSAGE TEXT}    %email%    ${random email}
    Wait Until Element Is Visible    ${RESET EMAIL SENT MESSAGE}
    ${text}    Get Text    ${RESET EMAIL SENT MESSAGE}
    ${replaced}    Replace String    ${text}    \n    ${SPACE}
    Should Match    ${replaced}    ${RESET EMAIL SENT MESSAGE TEXT}
    ${link}    Get Email Link    ${random email}    restore_password
    Go To    ${link}
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${EMPTY}
    Click Button    ${SAVE PASSWORD}
    Wait Until Element Is Visible    ${PASSWORD IS REQUIRED}
    Check Log In

works at restore password page with password input - after submit success
    [tags]    email
    ${random email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${random email}    ${password}
    ${link}    Get Email Link    ${random email}    activate
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${random email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${RESET EMAIL SENT MESSAGE TEXT}    Replace String    ${RESET EMAIL SENT MESSAGE TEXT}    %email%    ${random email}
    Wait Until Element Is Visible    ${RESET EMAIL SENT MESSAGE}
    ${text}    Get Text    ${RESET EMAIL SENT MESSAGE}
    ${replaced}    Replace String    ${text}    \n    ${SPACE}
    Should Match    ${replaced}    ${RESET EMAIL SENT MESSAGE TEXT}
    ${link}    Get Email Link    ${random email}    restore_password
    Go To    ${link}
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${password}
    Click Button    ${SAVE PASSWORD}
    Wait Until Elements Are Visible    ${RESET SUCCESS MESSAGE}    ${RESET SUCCESS LOG IN LINK}
    Check Log In