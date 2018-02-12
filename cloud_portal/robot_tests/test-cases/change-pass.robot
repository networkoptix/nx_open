*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${password}    ${BASE PASSWORD}
${email}       ${EMAIL VIEWER}
${url}         ${CLOUD TEST}

*** Keywords ***
Log In To Change Password Page
    Open Browser and go to URL    ${url}/account/password
    Log In    ${email}    ${password}    None
    Validate Log In
    Wait Until Elements Are Visible    ${CURRENT PASSWORD INPUT}    ${NEW PASSWORD INPUT}    ${CHANGE PASSWORD BUTTON}

*** Test Cases ***
password can be changed
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Input Text    ${NEW PASSWORD INPUT}    ${password}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    Your account is successfully saved
    Close Browser

password is actually changed, so login works with new password
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Input Text    ${NEW PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    Your account is successfully saved
    Log Out
    Validate Log Out

    Log In    ${email}    ${ALT PASSWORD}
    Validate Log In

    Go To    ${url}/account/password
    Wait Until Elements Are Visible    ${CURRENT PASSWORD INPUT}    ${NEW PASSWORD INPUT}    ${CHANGE PASSWORD BUTTON}
    Input Text    ${CURRENT PASSWORD INPUT}    ${ALT PASSWORD}
    Input Text    ${NEW PASSWORD INPUT}    ${password}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    Your account is successfully saved
    Close Browser

password change is not possible if old password is wrong
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    tjyjrsxhrsthr6
    Input Text    ${NEW PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    Cannot save password: Current password is incorrect
    Close Browser

more than 255 symbols can be entered in new password field and then are cut to 255
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${300CHARS}
    Input Text    ${NEW PASSWORD INPUT}    ${300CHARS}
    Textfield Should Contain    ${CURRENT PASSWORD INPUT}    ${255CHARS}
    Textfield Should Contain    ${NEW PASSWORD INPUT}    ${255CHARS}
    Close Browser

pressing Enter key saves data
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Input Text    ${NEW PASSWORD INPUT}    ${password}
    Press Key    ${NEW PASSWORD INPUT}    ${ENTER}
    Check For Alert    Your account is successfully saved
    Close Browser

pressing Tab key moves focus to the next element
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Press Key    ${CURRENT PASSWORD INPUT}    ${TAB}
    Element Should Be Focused    ${NEW PASSWORD INPUT}
    Input Text    ${NEW PASSWORD INPUT}    ${password}
    Press Key    ${NEW PASSWORD INPUT}    ${TAB}
    Element Should Be Focused    ${CHANGE PASSWORD BUTTON}
    Close Browser