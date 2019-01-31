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
Open New Browser On Failure
    Close Browser
    Open Browser and go to URL    ${url}

Restart
    Go To    ${url}
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log Out
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword Unless    ${status}    Log Out
    Go To    ${url}

*** Test Cases ***
can be opened in anonymous state
    [Tags]        Threaded
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${LOG IN MODAL}

can be closed by clicking on the X
    [tags]    C24212    Threaded
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Elements Are Visible    ${LOG IN MODAL}    ${BACKDROP}    ${LOG IN BUTTON}    ${EMAIL INPUT}    ${PASSWORD INPUT}    ${LOG IN CLOSE BUTTON}
    Click Button    ${LOG IN CLOSE BUTTON}
    Wait Until Page Does Not Contain Element    ${LOG IN MODAL}

allows to log in with existing credentials and to log out
    [tags]    C24212    C24213    Threaded
    Log In    ${email}    ${password}
    Validate Log In
    Log Out
    Validate Log Out

redirects to systems after log In
    [Tags]    Threaded
    Log In    ${email}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Location Should Be    ${url}/systems

after log In, display user's email and menu in top right corner
    [Tags]    Threaded
    Set Window Size    1920    1080
    Log In    ${email}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}/span[text()="${email}"]

allows log in with existing email in uppercase
    [Tags]    Threaded
    ${email uppercase}    Convert To Uppercase    ${email}
    Log In    ${email uppercase}    ${password}
    Validate Log In

allows log in with 'Remember Me checkmark' switched off
    [Tags]    Threaded
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Elements Are Visible    ${REMEMBER ME CHECKBOX VISIBLE}   ${EMAIL INPUT}    ${PASSWORD INPUT}    ${LOG IN BUTTON}
    Click Element    ${REMEMBER ME CHECKBOX VISIBLE}
    Checkbox Should Not Be Selected    ${REMEMBER ME CHECKBOX REAL}
    Log In    ${email}    ${password}    None
    Validate Log In

contains 'I forgot password' link that leads to Restore Password page with pre-filled email from log In form
    [Tags]    Threaded
    Log In    ${email}    'aderhgadehf'
    Wait Until Elements Are Visible    ${REMEMBER ME CHECKBOX VISIBLE}   ${EMAIL INPUT}    ${PASSWORD INPUT}    ${LOG IN BUTTON}    ${FORGOT PASSWORD}
    Click Link    ${FORGOT PASSWORD}
    Wait Until Element Is Visible    ${RESTORE PASSWORD EMAIL INPUT}
    Textfield Should Contain    ${RESTORE PASSWORD EMAIL INPUT}    ${email}

passes email from email input to Restore password page, even without clicking 'Log in' button
    [tags]    C41872    Threaded
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${EMAIL INPUT}
    Input Text    ${EMAIL INPUT}    ${email}
#the transition animations causes bad targeting on the link.  This is tentative.
    sleep    .15
    Wait Until Element Is Visible    ${FORGOT PASSWORD}
    Click Link    ${FORGOT PASSWORD}
    Wait Until Element Is Visible    ${RESTORE PASSWORD EMAIL INPUT}
    Textfield Should Contain    ${RESTORE PASSWORD EMAIL INPUT}    ${email}

shows non-activated user message when not activated at login; Resend activation button sends email
    [tags]    email    C41865    Threaded
    Go To    ${url}/register
    ${random email}    get random email    ${BASE EMAIL}
    Register    'mark'    'hamill'    ${random email}    ${password}
    Wait Until Element Is Visible    //h1[contains(@class,'process-success')]
    Log In    ${random email}    ${BASE PASSWORD}
    Wait Until Element Is Visible    ${RESEND ACTIVATION LINK BUTTON}
    Validate Register Email Received    ${random email}
    Click Link    ${RESEND ACTIVATION LINK BUTTON}
    Activate    ${random email}
    Log In    ${random email}    ${password}
    Validate Log In

displays password masked
    [tags]    Threaded
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${PASSWORD INPUT}
    ${input type}    Get Element Attribute    ${PASSWORD INPUT}    type
    Should Be Equal    '${input type}'    'password'

requires log In, if the user has just logged out and pressed back button in browser
    [tags]    Threaded
    Log In    ${email}    ${password}
    Validate Log In
    Log Out
    Go Back
    Wait Until Element Is Visible    ${LOG IN MODAL}

handles more than 255 symbols email and password
    [tags]    Threaded
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Elements Are Visible    ${EMAIL INPUT}    ${PASSWORD INPUT}
    Input Text    ${EMAIL INPUT}    ${300CHARS}
    Input Text    ${PASSWORD INPUT}    ${300CHARS}
    Textfield Should Contain    ${EMAIL INPUT}    ${255CHARS}
    Textfield Should Contain    ${PASSWORD INPUT}    ${255CHARS}

logout refreshes page
    [tags]    Threaded
    Log In    ${email}    ${password}
    Validate Log In
    Log Out
    Validate Log Out

# We don't actually allow copy of the password field at log in.
allows copy-paste in input fields
    [tags]    Threaded
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${EMAIL INPUT}
    Input Text    ${EMAIL INPUT}    Copy Paste Test
    ${locator}    Get WebElement    ${EMAIL INPUT}
    Copy Text    ${locator}
    Clear Element Text    ${locator}
    Paste Text    ${locator}
    Textfield Should Contain    ${EMAIL INPUT}    Copy Paste Test

should respond to Esc key and close dialog
    [tags]    Threaded
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${PASSWORD INPUT}
    Press Key    ${PASSWORD INPUT}    ${ESCAPE}
    Wait Until Element Is Not Visible    ${LOG IN MODAL}
    Element Should Not Be Visible    ${LOG IN MODAL}

should respond to Enter key and log in
    [tags]    Threaded
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Elements Are Visible    ${EMAIL INPUT}    ${PASSWORD INPUT}    ${REMEMBER ME CHECKBOX VISIBLE}    ${FORGOT PASSWORD}    ${LOG IN CLOSE BUTTON}
    Input Text    ${EMAIL INPUT}    ${email}
    Input Text    ${PASSWORD INPUT}    ${password}
    Wait Until Element Is Visible    ${LOG IN BUTTON}
    Press Key    ${PASSWORD INPUT}    ${ENTER}
    Validate Log In

should respond to Tab key
    [tags]    Threaded
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${EMAIL INPUT}
    Set Focus To Element    ${EMAIL INPUT}
    Press Key    ${EMAIL INPUT}    ${TAB}
    Element Should Be Focused    ${PASSWORD INPUT}

should respond to Space key and toggle checkbox
    [tags]    Threaded
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${REMEMBER ME CHECKBOX VISIBLE}
    Set Focus To Element    ${REMEMBER ME CHECKBOX REAL}
    Press Key    ${REMEMBER ME CHECKBOX REAL}    ${SPACEBAR}
    Checkbox Should Not Be Selected    ${REMEMBER ME CHECKBOX REAL}
    Press Key    ${REMEMBER ME CHECKBOX REAL}    ${SPACEBAR}
    Checkbox Should Be Selected    ${REMEMBER ME CHECKBOX REAL}

handles two tabs, updates second tab state if logout is done on first
    Go To    ${url}/register
    Wait Until Elements Are Visible    ${REGISTER FIRST NAME INPUT}    ${REGISTER LAST NAME INPUT}    ${REGISTER EMAIL INPUT}    ${REGISTER PASSWORD INPUT}    ${CREATE ACCOUNT BUTTON}
    Click Link    ${TERMS AND CONDITIONS LINK}
    Sleep    2    #This is specifically for Ubuntu Firefox because the new page isn't created fast enough and Get Window Handles only gets 1 item.
    ${tabs}    Get Window Handles
    Select Window    @{tabs}[1]
    Location Should Be    ${url}/content/eula
    Validate Log Out
    Sleep    5    #This is specifically for Ubuntu Firefox as the JS seems to load slowly and doesn't redirect correctly after login.
    Log In    ${email}    ${password}
    Validate Log In
    Select Window    @{tabs}[0]
    Location Should Be    ${url}/register
    Reload Page
    Wait Until Element Is Visible    ${CONTINUE BUTTON}
    Click Button    ${CONTINUE BUTTON}
    Wait Until Page Does Not Contain Element    ${CONTINUE MODAL}
    Validate Log In
    Log Out
    Validate Log Out
    ${tabs}    Get Window Handles
    Select Window    @{tabs}[1]
    Location Should Be    ${url}/systems
    Reload Page
    Wait Until Element Is Visible    ${LOG IN MODAL}

Log in more than 10 times
    [tags]    C42075    Threaded
    Go To    ${url}/register
    ${email}    Get Random Email    ${BASE EMAIL}
    Register    ${TEST FIRST NAME}    ${TEST LAST NAME}    ${email}    ${BASE PASSWORD}
    Activate    ${email}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Elements Are Visible    ${LOG IN MODAL}    ${BACKDROP}    ${LOG IN BUTTON}    ${EMAIL INPUT}    ${PASSWORD INPUT}    ${LOG IN CLOSE BUTTON}
    Input Text    ${EMAIL INPUT}    ${email}
    :FOR  ${x}  IN RANGE  10
    \  Input Text    ${PASSWORD INPUT}    incorrect
    \  Wait Until Element Is Visible    ${LOG IN BUTTON}
    \  Click Button    ${LOG IN BUTTON}
    \  Sleep    1
    Wait Until Element Is Visible    ${TOO MANY ATTEMPTS MESSAGE}
    Sleep    65
    Input Text    ${PASSWORD INPUT}    ${BASE PASSWORD}
    Wait Until Element Is Visible    ${LOG IN BUTTON}
    Click Button    ${LOG IN BUTTON}
    Validate Log In

User is logged out of browser after a password change in another browser
    [tags]    C41837
    Log In    ${email}    ${password}
    Validate Log In
    Open Browser and go to URL    ${url}
    Log In    ${email}    ${password}
    Validate Log In
    Switch Browser    1
    Go To    ${url}/account/password
    Sleep    1
    Wait Until Elements Are Visible    ${CURRENT PASSWORD INPUT}    ${NEW PASSWORD INPUT}    ${CHANGE PASSWORD BUTTON}
    Input Text    ${CURRENT PASSWORD INPUT}    ${password}
    Input Text    ${NEW PASSWORD INPUT}    ${ALT PASSWORD}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Switch Browser    2
    #wait for server to disconnect user
    sleep    30
    Wait Until Element Is Visible    ${LOG IN MODAL}
    Click Element    ${LOG IN CLOSE BUTTON}
    Validate Log Out

    Log In    ${email}    ${ALT PASSWORD}
    Validate Log In
    Go To    ${url}/account/password
    Sleep    1
    Wait Until Elements Are Visible    ${CURRENT PASSWORD INPUT}    ${NEW PASSWORD INPUT}    ${CHANGE PASSWORD BUTTON}
    Input Text    ${CURRENT PASSWORD INPUT}    ${ALT PASSWORD}
    Input Text    ${NEW PASSWORD INPUT}    ${password}
    Click Button    ${CHANGE PASSWORD BUTTON}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}