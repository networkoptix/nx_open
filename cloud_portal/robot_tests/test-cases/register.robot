*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${CLOUD TEST}

*** Keywords ***
Validate Register Success
    [arguments]    ${location}
    Wait Until Element Is Visible    ${ACCOUNT CREATION SUCCESS}
    Location Should Be    ${location}

Check Bad Email Input
    [arguments]    ${email}
    Wait Until Elements Are Visible    ${REGISTER EMAIL INPUT}    ${CREATE ACCOUNT BUTTON}
    Input Text    ${REGISTER EMAIL INPUT}    ${email}
    Click Button    ${CREATE ACCOUNT BUTTON}
    ${class}    Get Element Attribute    ${REGISTER EMAIL INPUT}/../..    class
    Should Contain    ${class}    has-error

*** Test Cases ***
should open register page in anonymous state by clicking Register button on top right corner
    Open Browser and go to URL    ${url}
    Wait Until Element Is Visible    ${CREATE ACCOUNT HEADER}
    Click Link    ${CREATE ACCOUNT HEADER}
    Location Should Be    ${url}/register
    Close Browser

should open register page from register success page by clicking Register button on top right corner
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    'mark'    'hamill'    ${email}    ${password}
    Activate    ${email}
    Wait Until Element Is Visible    ${CREATE ACCOUNT HEADER}
    Click Link    ${CREATE ACCOUNT HEADER}
    Location Should Be    ${url}/register
    Close Browser

should open register page in anonymous state by clicking Register button on homepage
    Open Browser and go to URL    ${url}
    Wait Until Element Is Visible    ${CREATE ACCOUNT BODY}
    Click Link    ${CREATE ACCOUNT BODY}
    Location Should Be    ${url}/register
    Close Browser

#I am assuming this means directly going to the /register url and not clicking a button
should open register page in anonymous state
    Open Browser and go to URL    ${url}/register
    Location should be    ${url}/register
    Close Browser

should register user with correct credentials
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Validate Register Success    ${url}/register/success
    Close Browser

should register user with cyrillic First and Last names and correct credentials
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    ${CYRILLIC NAME}    ${CYRILLIC NAME}    ${email}    ${password}
    Validate Register Success    ${url}/register/success
    Close Browser

should register user with smiley First and Last names and correct credentials
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    ${SMILEY NAME}    ${SMILEY NAME}    ${email}    ${password}
    Validate Register Success    ${url}/register/success
    Close Browser

should register user with glyph First and Last names and correct credentials
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    ${GLYPH NAME}    ${GLYPH NAME}    ${email}    ${password}
    Validate Register Success    ${url}/register/success
    Close Browser

should allow `~!@#$%^&*()_:\";\'{}[]+<>?,./ in First and Last name fields
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    ${SYMBOL NAME}    ${SYMBOL NAME}    ${email}    ${password}
    Validate Register Success    ${url}/register/success
    Close Browser

should allow !#$%&'*+-/=?^_`{|}~ in email field
    ${email}    Get Random Symbol Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Validate Register Success    ${url}/register/success
    Close Browser

should respond to Enter key and save data
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Wait Until Elements Are Visible    ${REGISTER FIRST NAME INPUT}    ${REGISTER LAST NAME INPUT}    ${REGISTER EMAIL INPUT}    ${REGISTER PASSWORD INPUT}
    Input Text    ${REGISTER FIRST NAME INPUT}    mark
    Input Text    ${REGISTER LAST NAME INPUT}    hamil
    Input Text    ${REGISTER EMAIL INPUT}    ${email}
    Input Text    ${REGISTER PASSWORD INPUT}    ${password}
    Press Key    ${REGISTER PASSWORD INPUT}    ${ENTER}
    Validate Register Success    ${url}/register/success
    Close Browser

should respond to Tab key
    Open Browser and go to URL    ${url}/register
    Wait Until Element Is Visible    ${REGISTER FIRST NAME INPUT}
    Element Should Be Focused    ${REGISTER FIRST NAME INPUT}
    Press Key    ${REGISTER FIRST NAME INPUT}    ${TAB}
    Element Should Be Focused    ${REGISTER LAST NAME INPUT}
    Press Key    ${REGISTER LAST NAME INPUT}    ${TAB}
    Element Should Be Focused    ${REGISTER EMAIL INPUT}
    Press Key    ${REGISTER EMAIL INPUT}    ${TAB}
    Element Should Be Focused    ${REGISTER PASSWORD INPUT}
    Press Key    ${REGISTER PASSWORD INPUT}    ${TAB}
    Element Should Be Focused    ${REGISTER SUBSCRIBE CHECKBOX}
    Press Key    ${REGISTER SUBSCRIBE CHECKBOX}    ${TAB}
    Element Should Be Focused    ${CREATE ACCOUNT BUTTON}
    Close Browser

should open Terms and conditions in a new page
    Open Browser and go to URL    ${url}/register
    Wait Until Element Is Visible    ${TERMS AND CONDITIONS LINK}
    Click Link    ${TERMS AND CONDITIONS LINK}
    ${tabs}    Get Window Handles
    Select Window    @{tabs}[1]
    Location Should Be    ${url}/content/eula
    Close Browser

should suggest user to log out, if he was logged in and goes to registration link
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL VIEWER}    ${password}
    Validate Log In
    Go To    ${url}/register
    Wait Until Elements Are Visible    ${LOGGED IN CONTINUE BUTTON}    ${LOGGED IN LOG OUT BUTTON}
    Close Browser

should display promo-block, if user goes to registration from native app
    Open Browser and go to URL    ${url}/register?from=client
    Wait Until Element Is Visible    ${PROMO BLOCK}
    Go To    ${url}/register?from=mobile
    Wait Until Element Is Visible    ${PROMO BLOCK}
    Close Browser

should not display promo-block, if user goes to registration not from native app
    Open Browser and go to URL    ${url}/register
    Wait Until Element Is Visible    ${REGISTER FIRST NAME INPUT}
    Element Should Not Be Visible    ${PROMO BLOCK}
    Close Browser

should remove promo-block on registration form successful submitting form when from=client
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register?from=client
    Register    mark    hamill    ${email}    ${password}
    Validate Register Success    ${url}/register/success?from=client
    Element Should Not Be Visible    ${PROMO BLOCK}
    Close Browser

should remove promo-block on registration form successful submitting form when from=mobile
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register?from=mobile
    Register    mark    hamill    ${email}    ${password}
    Validate Register Success    ${url}/register/success?from=mobile
    Element Should Not Be Visible    ${PROMO BLOCK}
    Close Browser

should not allow to access /register/success /activate/success by direct input
    Open Browser and go to URL    ${url}/register/success
    Wait Until Element Is Visible    ${JUMBOTRON}
    Location Should Be    ${url}/
    Go To    ${url}/activate/success
    Wait Until Element Is Visible    ${JUMBOTRON}
    Location Should Be    ${url}/
    Close Browser