*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${CLOUD TEST}

*** Keywords ***
Get Reset Password Link
    [arguments]    ${recipient}
    Open Mailbox    host=imap.gmail.com    password=qweasd!@#    port=993    user=noptixqa@gmail.com    is_secure=True
    ${email}    Wait For Email    recipient=${recipient}    timeout=120    subject=Reset your password
    ${links}    Get Links From Email    ${email}
    Close Mailbox
    Return From Keyword    @{links}[1]

*** Test Cases ***
should demand that email field is not empty
    Open Browser and go to URL    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Click Button    ${RESET PASSWORD BUTTON}
    ${class}    Get Element Attribute    ${RESTORE PASSWORD EMAIL INPUT}/../..    class
    Should Contain    ${class}    has-error
    Close Browser

should not succeed, if email is not registered
    Open Browser and go to URL    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${UNREGISTERED EMAIL}
    Click Button    ${RESET PASSWORD BUTTON}
    Check For Alert Dismissable    ${CANNOT SEND CONFIRMATION EMAIL}
    Close Browser

restores password
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    ${link}    Get Activation Link    ${email}
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    Wait Until Element Is Visible    ${RESET EMAIL SENT MESSAGE}
    Close Browser

should not allow to access /restore_password/sent /restore_password/success by direct input
    Open Browser and go to URL    ${url}/restore_password/sent
    Wait Until Element Is Visible    ${JUMBOTRON}
    Go To    ${url}/restore_password/success
    Wait Until Element Is Visible    ${JUMBOTRON}
    Close Browser

should be able to set new password (which is same as old), redirect
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    ${link}    Get Activation Link    ${email}
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${link}    Get Reset Password Link    ${email}
    Go To    ${link}[1]
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${password}
    Click Button    ${SAVE PASSWORD}
    Wait Until Elements Are Visible    ${RESET SUCCESS MESSAGE}    ${RESET SUCCESS LOG IN LINK}
    Close Browser

should set new password, login with new password
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    ${link}    Get Activation Link    ${email}
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${link}    Get Reset Password Link    ${email}
    Go To    ${link}[1]
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${SAVE PASSWORD}
    Wait Until Elements Are Visible    ${RESET SUCCESS MESSAGE}    ${RESET SUCCESS LOG IN LINK}
    Click Link    ${RESET SUCCESS LOG IN LINK}
    Log In    ${email}    ${ALT PASSWORD}    None
    Validate Log In
    Close Browser

should not allow to use one restore link twice
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    ${link}    Get Activation Link    ${email}
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${link}    Get Reset Password Link    ${email}
    Go To    ${link}[1]
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${SAVE PASSWORD}
    Wait Until Elements Are Visible    ${RESET SUCCESS MESSAGE}    ${RESET SUCCESS LOG IN LINK}
    Go To    ${link}[1]
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${SAVE PASSWORD}
    Check For Alert Dismissable    ${CANNOT SAVE PASSWORD: CODE USED/INCORRECT}
    Close Browser

should make not-activated user active by restoring password
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${link}    Get Reset Password Link    ${email}
    Go To    ${link}[1]
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${SAVE PASSWORD}
    Wait Until Elements Are Visible    ${RESET SUCCESS MESSAGE}    ${RESET SUCCESS LOG IN LINK}
    Click Link    ${RESET SUCCESS LOG IN LINK}
    Log In    ${email}    ${ALT PASSWORD}    None
    Validate Log In
    Close Browser

should allow logged in user visit restore password page
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}
    Validate Log In
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Close Browser

should prompt log user out if he visits restore password link from email
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}
    Validate Log In
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${link}    Get Reset Password Link    ${email}
    Go To    ${link}
    Wait Until Elements Are Visible    ${LOGGED IN CONTINUE BUTTON}    ${LOGGED IN LOG OUT BUTTON}
    Click Button    ${LOGGED IN CONTINUE BUTTON}
    Validate Log In
    Go To    ${link}
    Wait Until Elements Are Visible    ${LOGGED IN CONTINUE BUTTON}    ${LOGGED IN LOG OUT BUTTON}
    Click Button    ${LOGGED IN LOG OUT BUTTON}
    Validate Log Out
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Close Browser

should handle click I forgot my password link at restore password page
    Open Browser and go to URL    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Elements Are Visible    ${LOG IN MODAL}    ${EMAIL INPUT}    ${PASSWORD INPUT}    ${LOG IN BUTTON}    ${REMEMBER ME CHECKBOX}    ${FORGOT PASSWORD}    ${LOG IN CLOSE BUTTON}
    Click Link    ${FORGOT PASSWORD}
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Close Browser