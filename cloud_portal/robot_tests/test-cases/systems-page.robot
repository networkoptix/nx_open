*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Teardown     Close Browser
Suite Teardown    Run Keyword If Any Tests Failed    Clean up owner first/last name
Force Tags        system

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${ENV}

*** Keywords ***
Check Systems Text
    [arguments]    ${user}
    Log Out
    Validate Log Out
    Log In    ${user}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${AUTO TESTS USER}[text()='${TEST FIRST NAME} ${TEST LAST NAME}']
    Wait Until Element Is Not Visible    //h2[.='${YOUR SYSTEM TEXT}']

*** Test Cases ***
should show list of Systems
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${ACCOUNT DROPDOWN}    ${SYSTEMS TILE}

has system name, owner and OpenInNx button visible on systems page
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    Element Text Should Be    ${AUTO TESTS TITLE}    Auto Tests

should show Open in NX client button for online system
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}

should not show Open in NX client button for offline system
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTOTESTS OFFLINE}

should show system's state for systems if they are offline. Otherwise - button Open in Nx
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    ${systems}    Get WebElements    //div[@ng-repeat='system in systems | filter:searchSystems as filtered']
    Check Online Or Offline    ${systems}    ${AUTOTESTS OFFLINE TEXT}

should open system page (users list) when clicked on system
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    Click Element    ${AUTO TESTS TITLE}
    Verify In System    Auto Tests

should update owner name in systems list, if it's changed
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL OWNER}    ${password}    None
    Validate Log In
    Wait Until Elements Are Visible    ${ACCOUNT EMAIL}    ${ACCOUNT FIRST NAME}    ${ACCOUNT LAST NAME}    ${ACCOUNT SAVE}
    Wait Until Textfield Contains    ${ACCOUNT EMAIL}    ${EMAIL OWNER}
    Clear Element Text    ${ACCOUNT FIRST NAME}
    Input Text    ${ACCOUNT FIRST NAME}    newFirstName
    Clear Element Text    ${ACCOUNT LAST NAME}
    Input Text    ${ACCOUNT LAST NAME}    newLastName
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Log Out
    Validate Log Out
    Log In    ${EMAIL ADMIN}    ${password}
    Validate Log In
    Go To    ${url}/systems
    Wait Until Elements Are Visible    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    Element Text Should Be    ${AUTO TESTS USER}    newFirstName newLastName
    Close Browser

    Clean up owner first/last name

Should show your system for owner and owner name for non-owners
    [tags]    not-ready
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    Element Text Should Be    ${AUTO TESTS USER}    ${YOUR SYSTEM TEXT}
    :FOR    ${user}    IN    @{EMAILS LIST}
    \  Run Keyword Unless    "${user}"=="${EMAIL OWNER}"    Check Systems Text    ${user}