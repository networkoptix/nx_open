*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers


*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${ENV}

*** Keywords ***
Restart
    ${status}    Run Keyword And Return Status    Validate Log In
    Run Keyword If    ${status}    Log Out
    Go To    ${url}

Open New Browser On Failure
    Close Browser
    Open Browser and go to URL    ${url}

*** Test Cases ***
should demand that email field is not empty
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Click Button    ${RESET PASSWORD BUTTON}
    ${class}    Get Element Attribute    ${RESTORE PASSWORD EMAIL INPUT}/../..    class
    Should Contain    ${class}    has-error

restores password
    [tags]    email
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    ${link}    Get Email Link    ${email}    activate
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    Wait Until Element Is Visible    ${RESET EMAIL SENT MESSAGE}
    Location Should Be    ${url}/restore_password/sent

should not allow to access /restore_password/sent /restore_password/success by direct input
    Close Browser
    Open Browser and go to URL    ${url}
    Go To    ${url}/restore_password/sent
    Wait Until Element Is Visible    ${JUMBOTRON}
    Go To    ${url}/restore_password/success
    Wait Until Element Is Visible    ${JUMBOTRON}

should be able to set new password (which is same as old), redirect
    [tags]    email
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    ${link}    Get Email Link    ${email}    activate
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${link}    Get Email Link    ${email}    restore_password
    Go To    ${link}
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${password}
    Click Button    ${SAVE PASSWORD}
    Wait Until Elements Are Visible    ${RESET SUCCESS MESSAGE}    ${RESET SUCCESS LOG IN LINK}

should set new password, login with new password
    [tags]    email
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    ${link}    Get Email Link    ${email}    activate
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${link}    Get Email Link    ${email}    restore_password
    Go To    ${link}
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${SAVE PASSWORD}
    Wait Until Elements Are Visible    ${RESET SUCCESS MESSAGE}    ${RESET SUCCESS LOG IN LINK}
    Click Link    ${RESET SUCCESS LOG IN LINK}
    Log In    ${email}    ${ALT PASSWORD}    None
    Validate Log In

should not allow to use one restore link twice
    [tags]    email
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    ${link}    Get Email Link    ${email}    activate
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${link}    Get Email Link    ${email}    restore_password
    Go To    ${link}
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${SAVE PASSWORD}
    Wait Until Elements Are Visible    ${RESET SUCCESS MESSAGE}    ${RESET SUCCESS LOG IN LINK}
    Go To    ${link}
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${SAVE PASSWORD}
    Check For Alert Dismissable    ${CANNOT SAVE PASSWORD} ${CODE USED/INCORRECT}

should make not-activated user active by restoring password
    [tags]    email
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Validate Register Email Received    ${email}
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${link}    Get Email Link    ${email}    restore_password
    Go To    ${link}
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${SAVE PASSWORD}
    Wait Until Elements Are Visible    ${RESET SUCCESS MESSAGE}    ${RESET SUCCESS LOG IN LINK}
    Click Link    ${RESET SUCCESS LOG IN LINK}
    Log In    ${email}    ${ALT PASSWORD}    None
    Validate Log In

should allow logged in user visit restore password page
    [tags]    email
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}
    Validate Log In
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}

should prompt log user out if he visits restore password link from email
    [tags]    email
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}
    Validate Log In
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${link}    Get Email Link    ${email}    restore_password
    Go To    ${link}
    Wait Until Elements Are Visible    ${LOGGED IN CONTINUE BUTTON}    ${LOGGED IN LOG OUT BUTTON}
    Click Button    ${LOGGED IN CONTINUE BUTTON}
    Validate Log In
    Go To    ${link}
    Wait Until Elements Are Visible    ${LOGGED IN CONTINUE BUTTON}    ${LOGGED IN LOG OUT BUTTON}
    Click Button    ${LOGGED IN LOG OUT BUTTON}
    Validate Log Out
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}

should handle click I forgot my password link at restore password page
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Elements Are Visible    ${LOG IN MODAL}    ${EMAIL INPUT}    ${PASSWORD INPUT}    ${LOG IN BUTTON}    ${REMEMBER ME CHECKBOX}/..    ${FORGOT PASSWORD}    ${LOG IN CLOSE BUTTON}
    Click Link    ${FORGOT PASSWORD}
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}

#CLOUD-1747