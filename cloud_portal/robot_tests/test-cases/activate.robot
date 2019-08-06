*** Settings ***
Resource          ../resource.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers
Force Tags        Threaded File

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${ENV}
${symbol password}    pass!@#$%^&*()_-+=;:'"`~,./\|?[]{}

*** Keywords ***
Restart
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log Out
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword Unless    ${status}    Log Out
    Validate Log Out
    Go To    ${url}

Open New Browser On Failure
    Close Browser
    Open Browser and go to URL    ${url}

*** Test Cases ***
Register and Activate
    [tags]    email    C24211
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    'mark'    'hamill'    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In

allows register, activate, login with user with cyrillic First and Last names and correct credentials
    [tags]    C41863
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    ${CYRILLIC TEXT}    ${CYRILLIC TEXT}    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In

allows register, activate, login with user with smiley First and Last names and correct credentials
    [tags]    C41863
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    ${SMILEY TEXT}    ${SMILEY TEXT}    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In

allows register, activate, login with user with glyph First and Last names and correct credentials
    [tags]    C41863
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    ${GLYPH TEXT}    ${GLYPH TEXT}    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In

allows register, activate, login with `~!@#$%^&*()_:\";\'{}[]+<>?,./ in First and Last name fields
    [tags]    C41863
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    ${SYMBOL TEXT}    ${SYMBOL TEXT}    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In

allows register, activate, login with +!#$%'*-/=?^_`{|}~ in email field
#ampersand was removed from this test because imaplib could not handle it
    ${email}    Get Random Symbol Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In

allows register, activate, login with with leading space in email
    [tags]    C41864
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${SPACE}${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In

allows register, activate, login with with trailing space in email
    [tags]    C41864
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}${SPACE}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In

allows register, activate, login with pass!@#$%^&*()_-+=;:'"`~,./\|?[]{} password
    [tags]    C41861
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${symbol password}
    Activate    ${email}
    Log In    ${email}    ${symbol password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In

allows register, activate, login with qweasd 123
    [tags]    C41862
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${BASE PASSWORD}
    Activate    ${email}
    Log In    ${email}    ${BASE PASSWORD}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In

should show error if same link is used twice
    [tags]    email    C41566
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    'mark'    'hamill'    ${email}    ${password}
    ${link}    Get Email Link    ${email}    activate
    Go To    ${link}
    Wait Until Element Is Visible    ${ACTIVATION SUCCESS}
    Go To    ${link}
    Wait Until Element Is Visible    ${ALREADY ACTIVATED}

should save user data to user account correctly
    [tags]    email
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In
    Go To    ${url}/account
    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    mark
    Wait Until Textfield Contains    ${ACCOUNT LAST NAME}    hamill

should allow to enter more than 255 symbols in First and Last names and cut it to 255
    [tags]    email
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    ${300CHARS}    ${300CHARS}    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In
    Go To    ${url}/account
    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    ${255CHARS}
    Wait Until Textfield Contains    ${ACCOUNT LAST NAME}    ${255CHARS}

should trim leading and trailing spaces
    [tags]    email
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    ${SPACE}mark${SPACE}    ${SPACE}hamill${SPACE}    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In
    Go To    ${url}/account
    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    mark
    Wait Until Textfield Contains    ${ACCOUNT LAST NAME}    hamill

should allow activation, if user is registered by link /register/?from=client
    [tags]    email
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register?from=client
    Register    ${SPACE}mark${SPACE}    ${SPACE}hamill${SPACE}    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In

should allow activation, if user is registered by link /register/?from=mobile
    [tags]    email
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register?from=mobile
    Register    ${SPACE}mark${SPACE}    ${SPACE}hamill${SPACE}    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In

link works and suggests to log out user, if he was logged in, buttons operate correctly
    [tags]    email    C41564
    ${email1}    Get Random Email    ${BASE EMAIL}
    # This sleep is so that the emails don't get the same timestamp
    Sleep    .01
    ${email2}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email1}    ${password}
   ${link1}    Get Email Link    ${email1}    activate
    Go To    ${url}/register
    Register    mark    hamill    ${email2}    ${password}
    ${link2}    Get Email Link    ${email2}    activate
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${link1}
    Wait Until Page Contains Element    ${ACTIVATION SUCCESS}
    Wait Until Element Is Visible    ${LOGGED IN CONTINUE BUTTON}
    Click Button    ${LOGGED IN CONTINUE BUTTON}
    Validate Log In
    Log Out
    Validate Log Out
    Log In    ${email1}    ${password}
    Validate Log In
    Go To    ${link2}
    Wait Until Page Contains Element    ${ACTIVATION SUCCESS}
    Wait Until Element Is Visible    ${LOGGED IN LOG OUT BUTTON}
    Click Button    ${LOGGED IN LOG OUT BUTTON}
    Validate Log Out
    Log In    ${email2}    ${password}
    Validate Log In

#This is identical to "redirects to /activate and shows non-activated
#user message when not activated; Resend activation button sends email"
#in login-dialog
Logging in before activation shows resend email link and email can be sent again
    [tags]    email
    Go To    ${url}/register
    ${random email}    get random email    ${BASE EMAIL}
    Register    'mark'    'hamill'    ${random email}    ${BASE PASSWORD}
    Wait Until Element Is Visible    //h1[contains(@class,'process-success')]
    Log In    ${random email}    ${BASE PASSWORD}
    Wait Until Element Is Visible    ${RESEND ACTIVATION LINK BUTTON}
    Validate Register Email Received    ${random email}
    Click Link    ${RESEND ACTIVATION LINK BUTTON}
    Validate Register Email Received    ${random email}