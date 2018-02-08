*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${CLOUD TEST}

*** Keywords ***
Log in to System
    [arguments]    ${email}
    Go To    ${url}/systems/${AUTO TESTS URL}
    Log In    ${email}    ${password}    None
    Run Keyword If    '${email}' == '${EMAIL OWNER}'    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword If    '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword Unless    '${email}' == '${EMAIL OWNER}' or '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${OPEN IN NX BUTTON}

*** Test Cases ***
should show list of Systems
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${SYSTEMS TILE}
    Close Browser

should show Open in NX client button for online system
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    Close Browser

should not show Open in NX client button for offline system
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${AUTOTESTS OFFLINE}
    Close Browser

should show system's state for systems if they are offline. Otherwise - button Open in Nx
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    ${systems}    Get WebElements    //div[@ng-repeat='system in systems | filter:searchSystems as filtered']
    Check Online Or Offline    ${systems}
    Close Browser

should open system page (users list) when clicked on system
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    Click Element    ${AUTO TESTS TITLE}
    Verify In System    Auto Tests
    Close Browser

should update owner name in systems list, if it's changed
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL OWNER}    ${password}    None
    Validate Log In
    Wait Until Elements Are Visible    ${ACCOUNT FIRST NAME}    ${ACCOUNT LAST NAME}    ${ACCOUNT SAVE}
    Clear Element Text    ${ACCOUNT FIRST NAME}
    Input Text    ${ACCOUNT FIRST NAME}    newFirstName
    Clear Element Text    ${ACCOUNT LAST NAME}
    Input Text    ${ACCOUNT LAST NAME}    newLastName
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    Your account is successfully saved
    Go To    ${url}/systems
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    Element Text Should Be    ${AUTO TESTS USER}    newFirstName newLastName

    Go To    ${url}/account
    Wait Until Elements Are Visible    ${ACCOUNT FIRST NAME}    ${ACCOUNT LAST NAME}    ${ACCOUNT SAVE}
    Clear Element Text    ${ACCOUNT FIRST NAME}
    Input Text    ${ACCOUNT FIRST NAME}    testFirstName
    Clear Element Text    ${ACCOUNT LAST NAME}
    Input Text    ${ACCOUNT LAST NAME}    testLastName
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    Your account is successfully saved
    Close Browser