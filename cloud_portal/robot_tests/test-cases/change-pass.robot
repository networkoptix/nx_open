*** Settings ***
Resource          ../resource.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open browser and set user language to current
Suite Teardown    Clean up

*** Variables ***
${password}            ${BASE PASSWORD}
${symbol password}     pass!@#$%^&*()_-+=;:'"`~,./\|?[]{}
${space password}      qwea sd 123
${email}               ${EMAIL VIEWER}
${url}                 ${ENV}

*** Keywords ***
Open browser and set user language to current
    Open Browser and go to URL    ${url}
    Log In    ${email}    ${password}
    Validate Log In
    Log Out

Log In To Change Password Page
    Go To    ${url}/account/password
    Log In    ${email}    ${password}    None
    Validate Log In
    Wait Until Elements Are Visible    ${CURRENT PASSWORD INPUT}    ${NEW PASSWORD INPUT}    ${CHANGE PASSWORD BUTTON}

Reset user password to base
    [arguments]    ${email}    ${current password}
    Wait Until Elements Are Visible    ${CURRENT PASSWORD INPUT}    ${NEW PASSWORD INPUT}    ${CHANGE PASSWORD BUTTON}
    Input Text    ${CURRENT PASSWORD INPUT}    ${current password}
    Input Text    ${NEW PASSWORD INPUT}    ${BASE PASSWORD}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}

Restart
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Validate Log Out
    Go To    ${url}

Clean up
    Close Browser
    Restore Password    ${email}

Reset DB and Open New Browser On Failure
    Close Browser
    Restore Password    ${email}
    Open Browser and go to URL    ${url}

*** Test Cases ***
Can be accessed via dropdown or direct link
    [tags]    C41576
    Go To    ${url}/account/password
    Log In    ${email}    ${password}    None
    Validate Log In
    Wait Until Elements Are Visible    ${CURRENT PASSWORD INPUT}    ${NEW PASSWORD INPUT}    ${CHANGE PASSWORD BUTTON}
    Location Should Be    ${url}/account/password
    Go To    ${url}
    Wait Until Element Is Visible    ${AUTO TESTS TITLE}
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Click Button    ${ACCOUNT DROPDOWN}
    Wait Until Element Is Visible    ${CHANGE PASSWORD BUTTON DROPDOWN}
    Click Link    ${CHANGE PASSWORD BUTTON DROPDOWN}
    Wait Until Elements Are Visible    ${CURRENT PASSWORD INPUT}    ${NEW PASSWORD INPUT}    ${CHANGE PASSWORD BUTTON}
    Location Should Be    ${url}/account/password

password can be changed
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Input Text    ${NEW PASSWORD INPUT}    ${password}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}

password is actually changed, so login works with new password
    [tags]    C41576
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Input Text    ${NEW PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Log Out
    Validate Log Out
    Go To    ${url}/account/password
    Log In    ${email}    ${password}    None
    Wait Until Element Is Visible    ${WRONG PASSWORD MESSAGE}
    Log In    ${email}    ${ALT PASSWORD}    None
    Validate Log In
    Reset user password to base    ${email}    ${ALT PASSWORD}

password with symbols pass!@#$%^&*()_-+=;:'"`~,./\|?[]{} is valid
    [tags]    C41834
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Input Text    ${NEW PASSWORD INPUT}    ${symbol password}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Log Out
    Validate Log Out
    Go To    ${url}/account/password
    Log In    ${email}    ${password}    None
    Wait Until Element Is Visible    ${WRONG PASSWORD MESSAGE}
    Log In    ${email}    ${symbol password}    None
    Validate Log In
    Reset user password to base    ${email}    ${symbol password}

password with space in the middle is valid
    [tags]    C41835
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Input Text    ${NEW PASSWORD INPUT}    ${space password}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Log Out
    Validate Log Out
    Go To    ${url}/account/password
    Log In    ${email}    ${password}    None
    Wait Until Element Is Visible    ${WRONG PASSWORD MESSAGE}
    Log In    ${email}    ${space password}    None
    Validate Log In
    Reset user password to base    ${email}    ${space password}

more than 255 symbols can be entered in new password field and then are cut to 255
    [Tags]    Threaded
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
    [tags]    C41841
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Press Key    ${CURRENT PASSWORD INPUT}    ${TAB}
    Element Should Be Focused    ${NEW PASSWORD INPUT}
    Input Text    ${NEW PASSWORD INPUT}    ${password}
    Press Key    ${NEW PASSWORD INPUT}    ${TAB}
    Element Should Be Focused    ${CHANGE PASSWORD BUTTON}

displays password masked, shows password and changes eye icon when clicked
    [tags]    C41576
    Log In To Change Password Page
    ${input type}    Get Element Attribute    ${CURRENT PASSWORD INPUT}    type
    Should Be Equal    '${input type}'    'password'
    ${input type}    Get Element Attribute    ${NEW PASSWORD INPUT}    type
    Should Be Equal    '${input type}'    'password'
    Click Element    ${CHANGE PASS EYE ICON OPEN}
    Wait Until Element Is Visible    ${CHANGE PASS EYE ICON CLOSED}
    ${input type}    Get Element Attribute    ${NEW PASSWORD INPUT}    type
    Should Be Equal    '${input type}'    'text'
    Click Element    ${CHANGE PASS EYE ICON CLOSED}
    Wait Until Element Is Visible    ${CHANGE PASS EYE ICON OPEN}
    ${input type}    Get Element Attribute    ${NEW PASSWORD INPUT}    type
    Should Be Equal    '${input type}'    'password'

Password can't be changed if current password is not provided or incorrect
    [tags]    C41577
    Log In To Change Password Page
    Input Text    ${CURRENT PASSWORD INPUT}    ${EMPTY}
    Input Text    ${NEW PASSWORD INPUT}    ${password}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Input Text    ${NEW PASSWORD INPUT}    ${EMPTY}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Log Out
    Validate Log Out
    Log In    ${email}    ${password}
    Validate Log In