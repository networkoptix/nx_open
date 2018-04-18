*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Setup        Reset
Test Teardown     Run Keyword If Test Failed    Account Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Clean up

*** Variables ***
${password}    ${BASE PASSWORD}
${email}       ${EMAIL VIEWER}
${url}         ${ENV}

*** Keywords ***
Log In To Change Password Page
    Go To    ${url}/account/password
    Log In    ${email}    ${password}    None
    Validate Log In
    Wait Until Elements Are Visible    ${CURRENT PASSWORD INPUT}    ${NEW PASSWORD INPUT}    ${CHANGE PASSWORD BUTTON}

Reset
    ${status}    Run Keyword And Return Status    Validate Log In
    Run Keyword If    ${status}    Log Out
    Validate Log Out
    Go To    ${url}

Clean up
    Close Browser
    Run Keyword If Any Tests Failed    Clean up noperm first/last name

Account Failure
    Close Browser
    Clean up noperm first/last name
    Open Browser and go to URL    ${url}

*** Test Cases ***
password can be changed
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Input Text    ${NEW PASSWORD INPUT}    ${password}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}

password is actually changed, so login works with new password
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Input Text    ${NEW PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Log Out
    Validate Log Out

    Log In    ${email}    ${ALT PASSWORD}
    Validate Log In

    Go To    ${url}/account/password
    Wait Until Elements Are Visible    ${CURRENT PASSWORD INPUT}    ${NEW PASSWORD INPUT}    ${CHANGE PASSWORD BUTTON}
    Input Text    ${CURRENT PASSWORD INPUT}    ${ALT PASSWORD}
    Input Text    ${NEW PASSWORD INPUT}    ${password}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}

password change is not possible if old password is wrong
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    tjyjrsxhrsthr6
    Input Text    ${NEW PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    ${CANNOT SAVE PASSWORD} ${PASSWORD INCORRECT}

more than 255 symbols can be entered in new password field and then are cut to 255
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${300CHARS}
    Input Text    ${NEW PASSWORD INPUT}    ${300CHARS}
    Textfield Should Contain    ${CURRENT PASSWORD INPUT}    ${255CHARS}
    Textfield Should Contain    ${NEW PASSWORD INPUT}    ${255CHARS}

pressing Enter key saves data
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Input Text    ${NEW PASSWORD INPUT}    ${password}
    Press Key    ${NEW PASSWORD INPUT}    ${ENTER}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}

pressing Tab key moves focus to the next element
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Press Key    ${CURRENT PASSWORD INPUT}    ${TAB}
    Element Should Be Focused    ${NEW PASSWORD INPUT}
    Input Text    ${NEW PASSWORD INPUT}    ${password}
    Press Key    ${NEW PASSWORD INPUT}    ${TAB}
    Element Should Be Focused    ${CHANGE PASSWORD BUTTON}