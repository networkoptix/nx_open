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
    ${email}    Wait For Email    recipient=${recipient}    timeout=120
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
    Check For Alert Dismissable    Cannot send confirmation Email: Account does not exist
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
    ${link}    Get Reset Password Link    ${email}
    Go To    ${link}[1]
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Input Text    ${RESET PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${SAVE PASSWORD}
    Wait Until Elements Are Visible    ${RESET SUCCESS MESSAGE}    ${RESET SUCCESS LOG IN LINK}
    Element Should Be Visible    ${RESET SUCCESS MESSAGE}
    Click Link    ${RESET SUCCESS LOG IN LINK}
    Log In    ${EMAIL CHANGE PASS}    ${ALT PASSWORD}    None
    Validate Log In
    Close Browser