*** Settings ***
Resource          ../resource.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers


*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${ENV}

*** Keywords ***
Restart
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}

Open New Browser On Failure
    Close Browser
    Open Browser and go to URL    ${url}

*** Test Cases ***
restores password
    [tags]    email    C26260
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

can still log in if you don't finish the process
    [tags]    C41873
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Activate    ${email}
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    Log In    ${email}    ${password}
    Validate Log In
    Log Out
    Validate Log Out
    ${link}    Get Email Link    ${email}    restore_password
    Go To    ${link}
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Log In    ${email}    ${password}
    Validate Log In

should not allow to access /restore_password/sent /restore_password/success by direct input
    [tags]    Threaded
    Close Browser
    Open Browser and go to URL    ${url}
    Go To    ${url}/restore_password/sent
    Wait Until Element Is Visible    ${JUMBOTRON}
    Go To    ${url}/restore_password/success
    Wait Until Element Is Visible    ${JUMBOTRON}

should be able to set new password (which is same as old), redirect
    [tags]    email    C26260
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
    [tags]    email    C26260
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
    Log In    ${email}    ${password}    None
    Wait Until Element Is Visible    ${WRONG PASSWORD MESSAGE}
    Log In    ${email}    ${ALT PASSWORD}    None
    Validate Log In

displays password masked, shows password and changes eye icon when clicked
    [tags]    C26260    Threaded
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
    ${input type}    Get Element Attribute    ${RESET PASSWORD INPUT}    type
    Should Be Equal    '${input type}'    'password'
    Click Element    ${RESET EYE ICON OPEN}
    Wait Until Element Is Visible    ${RESET EYE ICON CLOSED}
    ${input type}    Get Element Attribute    ${RESET PASSWORD INPUT}    type
    Should Be Equal    '${input type}'    'text'
    Click Element    ${RESET EYE ICON CLOSED}
    Wait Until Element Is Visible    ${RESET EYE ICON OPEN}
    ${input type}    Get Element Attribute    ${RESET PASSWORD INPUT}    type
    Should Be Equal    '${input type}'    'password'

should not allow to use one restore link twice
    [tags]    email    C42079
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
    [tags]    email    C41871    Threaded
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
    [tags]    email    Threaded
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
    [tags]    Threaded
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Elements Are Visible    ${LOG IN MODAL}    ${EMAIL INPUT}    ${PASSWORD INPUT}    ${LOG IN BUTTON}    ${REMEMBER ME CHECKBOX VISIBLE}    ${FORGOT PASSWORD}    ${LOG IN CLOSE BUTTON}
    Click Link    ${FORGOT PASSWORD}
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}

Check restore password email links, colors, cloud name, and open link in new tab
    [tags]    C26260    Threaded
    ${random email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${random email}    ${password}
    ${link}    Get Email Link    ${random email}    activate
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${random email}
    Click Button    ${RESET PASSWORD BUTTON}
    Wait Until Element Is Visible    ${RESET EMAIL SENT MESSAGE}
    Open Mailbox    host=${BASE HOST}    password=${BASE EMAIL PASSWORD}    port=${BASE PORT}    user=${BASE EMAIL}    is_secure=True
    ${email}    Wait For Email    recipient=${random email}    timeout=120    status=UNSEEN
    ${email text}    Get Email Body    ${email}
    log    ${email text}
    Check Email Button    ${email text}    ${ENV}    ${THEME COLOR}
    Check Email Cloud Name    ${email text}    ${PRODUCT NAME}
    Check Email Subject    ${email}    ${RESET PASSWORD EMAIL SUBJECT}    ${BASE EMAIL}    ${BASE EMAIL PASSWORD}    ${BASE HOST}    ${BASE PORT}
    ${links}    Get Links From Email    ${email}
    @{expected links}    Set Variable    ${SUPPORT URL}    ${WEBSITE URL}    ${ENV}    ${ENV}/restore_password
    : FOR    ${link}  IN  @{links}
    \    check in list    ${expected links}    ${link}
    Delete Email    ${email}
    Close Mailbox