*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${email}    ${EMAIL OWNER}
${email upppercase}    NOPTIXQA+OWNER@GMAIL.COM
${password}    ${BASE PASSWORD}
${url}         ${CLOUD TEST}

*** Test Cases ***
can be opened in anonymous state
    Open Browser and Go To URL    ${url}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${LOG IN MODAL}
    Element Should be Visible    ${LOG IN MODAL}
    Close Browser

can be closed after clicking on background
    Open Browser and Go To URL    ${url}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${LOG IN MODAL}
    Element Should be Visible    //div[uib-modal-backdrop='modal-backdrop']
    Click Element    //div[uib-modal-backdrop='modal-backdrop']
    Wait Until Page Does Not Contain Element    ${LOG IN MODAL}
    Page Should Not Contain Element    ${LOG IN MODAL}
    Close Browser

allows to log in with existing credentials and to log out
    Open Browser and go to URL    ${url}
    Log In    ${email}    ${password}
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Click Element    ${ACCOUNT DROPDOWN}
    Wait Until Element Is Visible    ${LOG OUT BUTTON}
    Click Link    ${LOG OUT BUTTON}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Element Should Be Visible    ${LOG IN NAV BAR}
    Close Browser

redirects to systems after log In
    Open Browser and go to URL    ${url}
    Log In    ${email}    ${password}
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    ${current page}    Get Location
    Should Be True    '${current page}' == '${url}/systems'
    Close Browser

redirects to systems after log In, (EXPECTED FAILURE)
    Open Browser and go to URL    ${url}
    Log In    ${email}    ${password}
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    ${current page}    Get Location
    Should Be True    '${current page}' == '${url}/system'
    Close Browser

after log In, display user's email and menu in top right corner
    Open Browser and go to URL    ${url}
    Log In    ${email}    ${password}
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Element Text Should Be    ${ACCOUNT DROPDOWN}    ${email}
    Close Browser

valid but unregistered email shows error message
    Open Browser and go to URL    ${url}
    Log In    ${GOOD EMAIL UNREGISTERED}    ${password}
    wait until element is visible    ${ALERT}
    Element Should Be Visible    ${ALERT}
    Close Browser

allows log in with existing email in uppercase
    Open Browser and go to URL    ${url}
    Log In    ${email upppercase}    ${password}
    Validate Log In
    Close Browser

rejects log in with wrong password
    Open Browser and go to URL    ${url}
    Log In    ${email}    'arthahrtrthjsrtjy'
    wait until element is visible    ${ALERT}
    Element Should Be Visible    ${ALERT}
    Close Browser

rejects log in without password
    Open Browser and go to URL    ${url}
    Log In    ${email}    ${EMPTY}
    ${class}    Get Element Attribute    ${PASSWORD INPUT}/..    class
    Should Contain    ${class}    has-error
    Close Browser

rejects log in without both email and password
    Open Browser and go to URL    ${url}
    Log In    ${EMPTY}    ${EMPTY}
    ${class}    Get Element Attribute    ${PASSWORD INPUT}/..    class
    Should Contain    ${class}    has-error
    ${class}    Get Element Attribute    ${EMAIL INPUT}/..    class
    Should Contain    ${class}    has-error
    Close Browser

rejects log in without both email and password, (EXPECTED FAILURE)
    Open Browser and go to URL    ${url}
    Log In    ${EMPTY}    ${EMPTY}
    ${class}    Get Element Attribute    ${PASSWORD INPUT}/..    class
    Should Contain    ${class}    has-eror
    ${class}    Get Element Attribute    ${EMAIL INPUT}/..    class
    Should Contain    ${class}    has-error
    Close Browser

rejects log in with email in non-email format but with password
    Open Browser and go to URL    ${url}
    Log In    ${INVALID}    ${password}
    ${class}    Get Element Attribute    ${EMAIL INPUT}/..    class
    Should Contain    ${class}    has-error
    Close Browser

shows red outline if field is wrong/empty after blur
    Open Browser and go to URL    ${url}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${EMAIL INPUT}
    Click Element    ${EMAIL INPUT}
    Wait Until Element Is Visible    ${PASSWORD INPUT}
    Click Element    ${PASSWORD INPUT}
    ${class}    Get Element Attribute    ${EMAIL INPUT}/..    class
    Should Contain    ${class}    has-error
    Click Element    ${REMEMBER ME CHECKBOX}
    ${class}    Get Element Attribute    ${PASSWORD INPUT}/..    class
    Should Contain    ${class}    has-error
    Close Browser

allows log in with 'Remember Me checkmark' switched off
    Open Browser and go to URL    ${url}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${REMEMBER ME CHECKBOX}
    Click Element    ${REMEMBER ME CHECKBOX}
    Checkbox Should Not Be Selected    ${REMEMBER ME CHECKBOX}
    Wait Until Element Is Visible    ${EMAIL INPUT}
    input text    ${EMAIL INPUT}    ${email}
    Wait Until Element Is Visible    ${PASSWORD INPUT}
    input text    ${PASSWORD INPUT}    ${password}
    Wait Until Element Is Visible    ${LOG IN BUTTON}
    click button    ${LOG IN BUTTON}
    Validate Log In
    Close Browser

contains 'I forgot password' link that leads to Restore Password page with pre-filled email from log In form
    Open Browser and go to URL    ${url}
    Log In    ${email}    'aderhgadehf'
    Wait Until Element Is Visible    ${FORGOT PASSWORD}
    Click Link    ${FORGOT PASSWORD}
    Wait Until Element Is Visible    ${RESET PASSWORD EMAIL INPUT}
    Textfield Should Contain    ${RESET PASSWORD EMAIL INPUT}    ${email}
    Close Browser

passes email from email input to Restore password page, even without clicking 'Log in' button
    Open Browser and go to URL    ${url}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${EMAIL INPUT}
    Input Text    ${EMAIL INPUT}    ${email}
    Wait Until Element Is Visible    ${FORGOT PASSWORD}
    Click Element    ${FORGOT PASSWORD}
    Wait Until Element Is Visible    ${RESET PASSWORD EMAIL INPUT}
    Textfield Should Contain    ${RESET PASSWORD EMAIL INPUT}    ${email}
    Close Browser

redirects to /activate and shows non-activated user message when not activated; Resend activation button sends email
    Open Browser and go to URL    ${url}/register
# Created this keyword myself in python.  It's located in browsermanagement.py line 255
    ${random email}    get random email
    Register    'mark'    'hamill'    ${random email}    ${GOOD PASSWORD}
    Wait Until Element Is Visible    //h1[contains(@class,'process-success')]
    Log In    ${random email}    ${GOOD PASSWORD}
    Wait Until Element Is Visible    ${RESEND ACTIVATION LINK BUTTON}
    ${current page}    Get Location
    Should Be True    '${current page}' == '${url}/activate'
    Validate Register Email Received    ${random email}
    Click Button    ${RESEND ACTIVATION LINK BUTTON}
    Validate Register Email Received    ${random email}
    Close Browser

displays password masked
    Open Browser and go to URL    ${url}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${PASSWORD INPUT}
    ${input type}    Get Element Attribute    ${PASSWORD INPUT}    type
    Should Be Equal    '${input type}'    'password'
    Close Browser

requires log In, if the user has just logged out and pressed back button in browser
    Open Browser and go to URL    ${url}
    Log In    ${email}    ${password}
    Validate Log In
    Log Out
    Go Back
    Validate Log Out
    Close Browser

handles more than 256 symbols email and password
    Open Browser and go to URL    ${url}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${EMAIL INPUT}
    Input Text    ${EMAIL INPUT}    ${300CHARS}
    Wait Until Element Is Visible    ${PASSWORD INPUT}
    Input Text    ${PASSWORD INPUT}    ${300CHARS}
    Textfield Should Contain    ${EMAIL INPUT}    ${255CHARS}
    Textfield Should Contain    ${PASSWORD INPUT}    ${255CHARS}
    Close Browser

logout refreshes page
    Open Browser and go to URL    ${url}
    Log In    ${email}    ${password}
    Validate Log In
    Log Out
    Validate Log Out
    Close Browser

# We don't actually allow copy of the password field...
allows copy-paste in input fields
    Open Browser and go to URL    ${url}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${EMAIL INPUT}
    Input Text    ${EMAIL INPUT}    Copy Paste Test
    ${locator}    Get WebElement    ${EMAIL INPUT}
    Copy Text    ${locator}
    Clear Element Text    ${locator}
    Paste Text    ${locator}
    Textfield Should Contain    ${EMAIL INPUT}    Copy Paste Test
    Close Browser

should respond to Esc key and close dialog
    Open Browser and go to URL    ${url}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${PASSWORD INPUT}
    Press Key    ${PASSWORD INPUT}    \\27
    Wait Until Element Is Not Visible    ${LOG IN MODAL}
    Element Should Not Be Visible    ${LOG IN MODAL}
    Close Browser

should respond to Enter key and log in
    Open Browser and go to URL    ${url}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${EMAIL INPUT}
    Input Text    ${EMAIL INPUT}    ${email}
    Wait Until Element Is Visible    ${PASSWORD INPUT}
    Input Text    ${PASSWORD INPUT}    ${password}
    Press Key    ${PASSWORD INPUT}    \\13
    Validate Log In
    Close Browser

should respond to Tab key
    Open Browser and go to URL    ${url}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${EMAIL INPUT}
    Set Focus To Element    ${EMAIL INPUT}
    Press Key    ${EMAIL INPUT}    \\9
    Element Should Be Focused    ${PASSWORD INPUT}
    Close Browser

should respond to Space key and toggle checkbox
    Open Browser and go to URL    ${url}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${REMEMBER ME CHECKBOX}
    Set Focus To Element    ${REMEMBER ME CHECKBOX}
    Press Key    ${REMEMBER ME CHECKBOX}    \\32
    Checkbox Should Not Be Selected    ${REMEMBER ME CHECKBOX}
    Press Key    ${REMEMBER ME CHECKBOX}    \\32
    Checkbox Should Be Selected    ${REMEMBER ME CHECKBOX}
    Close Browser

handles two tabs, updates second tab state if logout is done on first
    Open Browser and go to URL    ${url}/register
    Wait Until Element Is Visible    ${TERMS AND CONDITIONS LINK}
    Click Link    ${TERMS AND CONDITIONS LINK}
    ${tabs}    Get Window Titles
    Select Window    @{tabs}[1]
    Location Should Be    ${url}/content/eula
    Validate Log Out
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
    ${tabs}    Get Window Titles
    Select Window    @{tabs}[1]
    Location Should Be    ${url}/systems
    Reload Page
    Validate Log Out
    Close Browser