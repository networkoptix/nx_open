*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
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

Reset DB and Open New Browser On Failure
    Close Browser
    Clean up random emails
    Clean up email noperm
    Open Browser and go to URL    ${url}

*** Test Cases ***
should open register page in anonymous state by clicking Register button on top right corner
    Go To    ${url}
    Wait Until Element Is Visible    ${CREATE ACCOUNT HEADER}
    Click Link    ${CREATE ACCOUNT HEADER}
    Location Should Be    ${url}/register

should open register page from register success page by clicking Register button on top right corner
    [tags]    email
    ${email}    Get Random Email        ${BASE EMAIL}
    Go To    ${url}/register
    Register    'mark'    'hamill'    ${email}    ${password}
    Activate    ${email}
    Wait Until Element Is Visible    ${CREATE ACCOUNT HEADER}
    Click Link    ${CREATE ACCOUNT HEADER}
    Location Should Be    ${url}/register

should open register page in anonymous state by clicking Register button on homepage
    Wait Until Element Is Visible    ${CREATE ACCOUNT BODY}
    Click Link    ${CREATE ACCOUNT BODY}
    Location Should Be    ${url}/register

#I am assuming this means directly going to the /register url and not clicking a button
should open register page in anonymous state
    [tags]    C24211
    Go To    ${url}/register
    Location should be    ${url}/register
    Wait Until Element Is Visible    //form[@name="registerForm"]

should register user with correct credentials
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Validate Register Success

should allow !#$%&'*+-/=?^_`{|}~ in email field
    [documentation]    This is here because testing activation with the '&' freaks out Python's imaplib so we test that our form accepts it.
    ${email}    Get Random Symbol Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Validate Register Success

with valid inputs no errors are displayed
    [tags]    C41557
    ${email}    Get Random Email    ${BASE EMAIL}
    Wait Until Element Is Visible    ${CREATE ACCOUNT HEADER}
    Click Link    ${CREATE ACCOUNT HEADER}
    Wait Until Elements Are Visible    ${REGISTER FIRST NAME INPUT}    ${REGISTER LAST NAME INPUT}    ${REGISTER PASSWORD INPUT}    ${CREATE ACCOUNT BUTTON}
    Input Text    ${REGISTER FIRST NAME INPUT}    ${TEST FIRST NAME}
    Input Text    ${REGISTER LAST NAME INPUT}    ${TEST LAST NAME}
    ${read only}    Run Keyword And Return Status    Wait Until Element Is Visible    ${REGISTER EMAIL INPUT LOCKED}
    ${email}    Get Random Email    ${BASE EMAIL}
    Run Keyword Unless    ${read only}    Input Text    ${REGISTER EMAIL INPUT}    ${email}
    Input Text    ${REGISTER PASSWORD INPUT}    ${password}
    Click Element    ${TERMS AND CONDITIONS CHECKBOX}
    Click Element    ${REGISTER FORM}
    @{list}    Set Variable    ${FIRST NAME IS REQUIRED}    ${LAST NAME IS REQUIRED}    ${LAST NAME IS REQUIRED}    ${EMAIL IS REQUIRED}    ${PASSWORD SPECIAL CHARS}    ${PASSWORD TOO SHORT}    ${PASSWORD TOO COMMON}    ${PASSWORD IS WEAK}    ${EMAIL INVALID}
    : FOR    ${element}    IN    @{list}
    \    Element Should Not Be Visible    ${element}

displays password masked, shows password and changes eye icon when clicked
    [tags]    C24211
    Go To    ${url}/register
    Wait Until Elements Are Visible    ${REGISTER PASSWORD INPUT}    ${REGISTER EYE ICON OPEN}
    ${input type}    Get Element Attribute    ${REGISTER PASSWORD INPUT}    type
    Should Be Equal    '${input type}'    'password'
    Click Element    ${REGISTER EYE ICON OPEN}
    Wait Until Element Is Visible    ${REGISTER EYE ICON CLOSED}
    ${input type}    Get Element Attribute    ${REGISTER PASSWORD INPUT}    type
    Should Be Equal    '${input type}'    'text'
    Click Element    ${REGISTER EYE ICON CLOSED}
    Wait Until Element Is Visible    ${REGISTER EYE ICON OPEN}
    ${input type}    Get Element Attribute    ${REGISTER PASSWORD INPUT}    type
    Should Be Equal    '${input type}'    'password'

should respond to Enter key and save data
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Wait Until Elements Are Visible    ${REGISTER FIRST NAME INPUT}    ${REGISTER LAST NAME INPUT}    ${REGISTER EMAIL INPUT}    ${REGISTER PASSWORD INPUT}
    Input Text    ${REGISTER FIRST NAME INPUT}    mark
    Input Text    ${REGISTER LAST NAME INPUT}    hamil
    Input Text    ${REGISTER EMAIL INPUT}    ${email}
    Input Text    ${REGISTER PASSWORD INPUT}    ${password}
    Click Element    ${TERMS AND CONDITIONS CHECKBOX}
    Press Key    ${REGISTER PASSWORD INPUT}    ${ENTER}
    Validate Register Success

should respond to Tab key
    [tags]    C41867
    Wait Until Element Is Visible    ${CREATE ACCOUNT HEADER}
    Click Link    ${CREATE ACCOUNT HEADER}
    Wait Until Elements Are Visible    ${REGISTER FIRST NAME INPUT}    ${REGISTER LAST NAME INPUT}    ${REGISTER EMAIL INPUT}    ${REGISTER PASSWORD INPUT}
    Element Should Be Focused    ${REGISTER FIRST NAME INPUT}
    Press Key    ${REGISTER FIRST NAME INPUT}    ${TAB}
    Element Should Be Focused    ${REGISTER LAST NAME INPUT}
    Press Key    ${REGISTER LAST NAME INPUT}    ${TAB}
    Element Should Be Focused    ${REGISTER EMAIL INPUT}
    Press Key    ${REGISTER EMAIL INPUT}    ${TAB}
    Element Should Be Focused    ${REGISTER PASSWORD INPUT}
    Press Key    ${REGISTER PASSWORD INPUT}    ${TAB}
    Element Should Be Focused    ${TERMS AND CONDITIONS CHECKBOX}

    Press Key    ${TERMS AND CONDITIONS CHECKBOX}    ${SPACEBAR}
    Wait Until Page Contains Element    //form[@name='registerForm']//input[contains(@class, "ng-not-empty") and @ng-model='account.accept']
    Press Key    ${TERMS AND CONDITIONS CHECKBOX}    ${SPACEBAR}
    Wait Until Page Contains Element    //form[@name='registerForm']//input[contains(@class, "ng-empty") and @ng-model='account.accept']

    Press Key    ${TERMS AND CONDITIONS CHECKBOX}    ${TAB}
    Press Key    ${TERMS AND CONDITIONS LINK}    ${ENTER}
    Element Should Be Focused    ${TERMS AND CONDITIONS LINK}
    ${tabs}    Get Window Handles
    Select Window    @{tabs}[1]
    Location Should Be    ${url}/content/eula
    Select Window    @{tabs}[0]
    Press Key    ${TERMS AND CONDITIONS LINK}    ${TAB}
    Element Should Be Focused    ${PRIVACY POLICY LINK}
    Press Key    ${PRIVACY POLICY LINK}    ${ENTER}
    ${tabs}    Get Window Handles
    Select Window    @{tabs}[2]
    Location Should Be    ${url}/content/privacy
    Select Window    @{tabs}[0]

    Press Key    ${PRIVACY POLICY LINK}    ${TAB}
    Element Should Be Focused    ${CREATE ACCOUNT BUTTON}
    Press Key    ${CREATE ACCOUNT BUTTON}    ${ENTER}
    Wait Until Elements Are Visible    ${FIRST NAME IS REQUIRED}    ${LAST NAME IS REQUIRED}    ${EMAIL IS REQUIRED}    ${PASSWORD IS REQUIRED}

should open Terms and conditions in a new page
    [tags]    C41558
    Go To    ${url}/register
    Wait Until Element Is Visible    ${TERMS AND CONDITIONS LINK}
    Click Link    ${TERMS AND CONDITIONS LINK}
    Sleep    2    #This is specifically for Firefox
    ${tabs}    Get Window Handles
    Select Window    @{tabs}[1]
    Location Should Be    ${url}/content/eula

should open Privacy Policy in a new page
    [tags]    C41558
    Go To    ${url}/register
    Wait Until Element Is Visible    ${PRIVACY POLICY LINK}
    Click Link    ${PRIVACY POLICY LINK}
    Sleep    2    #This is specifically for Firefox
    ${windows}    Get Window Handles
    Select Window    @{windows}[2]
    Location Should Be    ${PRIVACY_POLICY_URL}

should suggest user to log out, if he was logged in and goes to registration link
    Log In    ${EMAIL VIEWER}    ${password}
    Validate Log In
    Go To    ${url}/register
    Wait Until Elements Are Visible    ${LOGGED IN CONTINUE BUTTON}    ${LOGGED IN LOG OUT BUTTON}
    Click Button    ${LOGGED IN CONTINUE BUTTON}
    Go To    ${url}/register
    Wait Until Elements Are Visible    ${LOGGED IN CONTINUE BUTTON}    ${LOGGED IN LOG OUT BUTTON}
    Click Button    ${LOGGED IN LOG OUT BUTTON}
    Validate Log Out

should display promo-block, if user goes to registration from native app
    Go To    ${url}/register?from=client
    Wait Until Element Is Visible    ${PROMO BLOCK}
    Go To    ${url}/register?from=mobile
    Wait Until Element Is Visible    ${PROMO BLOCK}

should not display promo-block, if user goes to registration not from native app
    Go To    ${url}/register
    Wait Until Element Is Visible    ${REGISTER FIRST NAME INPUT}
    Element Should Not Be Visible    ${PROMO BLOCK}

should remove promo-block on registration form successful submitting form when from=client
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register?from=client
    Register    mark    hamill    ${email}    ${password}
    Validate Register Success    ${url}/register/success?from=client
    Element Should Not Be Visible    ${PROMO BLOCK}

should remove promo-block on registration form successful submitting form when from=mobile
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register?from=mobile
    Register    mark    hamill    ${email}    ${password}
    Validate Register Success    ${url}/register/success?from=mobile
    Element Should Not Be Visible    ${PROMO BLOCK}

should not allow to access /register/success /activate/success by direct input
    Close Browser
    Open Browser and go to URL    ${url}/register/success
    Wait Until Element Is Visible    ${JUMBOTRON}
    Location Should Be    ${url}/
    Go To    ${url}/activate/success
    Wait Until Element Is Visible    ${JUMBOTRON}
    Location Should Be    ${url}/

Cannot register email that is already registered
    [tags]    C41563
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Wait Until Element Is Visible    //form[@name="registerForm"]//span[@ng-if="registerForm.registerEmail.$error.alreadyExists" and text()="${EMAIL ALREADY REGISTERED TEXT}"]

Cannot register email that is already activated
    [tags]    C41563
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Activate    ${email}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Wait Until Element Is Visible    //form[@name="registerForm"]//span[@ng-if="registerForm.registerEmail.$error.alreadyExists" and text()="${EMAIL ALREADY REGISTERED TEXT}"]

Check registration email links
    [tags]    C24211
    ${random email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    ${TEST FIRST NAME}    ${TEST LAST NAME}    ${random email}    ${password}
    Open Mailbox    host=${BASE HOST}    password=${BASE EMAIL PASSWORD}    port=${BASE PORT}    user=${BASE EMAIL}    is_secure=True



    ${email}    Wait For Email    recipient=${random email}    timeout=120    status=UNSEEN
    ${email text}    Get Email Body    ${email}
    Log   ${email text}
    Check Email Body    ${email text}    ${email color}    ${TEST FIRST NAME}    ${TEST LAST NAME}



    Check Email Subject    ${email}    ${ACTIVATE YOUR ACCOUNT EMAIL SUBJECT}    ${BASE EMAIL}    ${BASE EMAIL PASSWORD}    ${BASE HOST}    ${BASE PORT}
    ${INVITED TO SYSTEM EMAIL SUBJECT UNREGISTERED}    Replace String    ${INVITED TO SYSTEM EMAIL SUBJECT UNREGISTERED}    {{message.sharer_name}}    ${TEST FIRST NAME} ${TEST LAST NAME}
    ${INVITED TO SYSTEM EMAIL SUBJECT UNREGISTERED}    Replace String    ${INVITED TO SYSTEM EMAIL SUBJECT UNREGISTERED}    %PRODUCT_NAME%    Nx Cloud
    ${links}    Get Links From Email    ${email}
    @{expected links}    Set Variable    ${SUPPORT_URL}    ${WEBSITE_URL}    ${ENV}    ${ENV}/activate
    : FOR    ${link}  IN  @{links}
    \    check in list    ${expected links}    ${link}
    Delete Email    ${email}
    Close Mailbox