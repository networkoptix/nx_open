*** Settings ***
Resource          ../resource.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers
Force Tags        system

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${ENV}

*** Keywords ***
Check Systems Text
    [arguments]    ${user}
    Sleep    1
    Log Out
    Validate Log Out
    Log In    ${user}    ${password}
    Validate Log In
    Wait Until Page Contains Element    ${AUTO TESTS USER}[text()='${TEST FIRST NAME} ${TEST LAST NAME}']
    Wait Until Element Is Not Visible    //h2[.='${YOUR SYSTEM TEXT}']

Reset DB and Open New Browser On Failure
    Close Browser
    Reset user owner first/last name
    Open Browser and go to URL    ${url}

Restart
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}

*** Test Cases ***
should show list of Systems
    [tags]    C41893
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${ACCOUNT DROPDOWN}    ${SYSTEMS TILE}

has system name, owner and OpenInNx button visible on systems page
    [tags]    C41893
    Log In    ${EMAIL OWNER}    ${password}
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    Element Text Should Be    ${AUTO TESTS TITLE}    Auto Tests

should show Open in NX client button for online system
    [tags]    C41893
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}

should not show Open in NX client button for offline system
    [tags]    C41893
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTOTESTS OFFLINE}

should show system's state for systems if they are offline. Otherwise - button Open in Nx
    [tags]    C41893
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    ${systems}    Get WebElements    //div[@ng-repeat='system in systems | filter:searchSystems as filtered track by system.id']
    Check Online Or Offline    ${systems}    ${AUTOTESTS OFFLINE TEXT}

should show the no systems connected message when you have no systems
    [tags]    C41866
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${YOU HAVE NO SYSTEMS}

should show system name in header with no dropdown if user has only one system
    [tags]    C41569
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO_TESTS SYSTEM ID}
    Share To    ${EMAIL NOPERM}    ${VIEWER TEXT}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Log Out
    Validate Log Out
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${SYSTEM NAME AUTO TESTS HEADER}
    Log Out
    Validate Log Out
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO_TESTS SYSTEM ID}
    Remove User Permissions    ${EMAIL NOPERM}

should show the system page instead of all systems when user only has one
    [tags]    C41878
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO_TESTS SYSTEM ID}
    Share To    ${EMAIL NOPERM}    ${VIEWER TEXT}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Log Out
    Validate Log Out
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${SYSTEM NAME}
    Log Out
    Validate Log Out
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO_TESTS SYSTEM ID}
    Remove User Permissions    ${EMAIL NOPERM}

should open system page (users list) when clicked on system
    [tags]    C41893
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    # Sometimes the name fields refill if you empty them too fast
    sleep    2
    Wait Until Page Does Not Contain Element    //div[@class='preloader']
    Click Element    ${AUTO TESTS TITLE}
    Verify In System    Auto Tests

Should show your system for owner and owner name for non-owners
    [tags]    C41893
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    Element Text Should Be    ${AUTO TESTS USER}    ${YOUR SYSTEM TEXT}
    :FOR    ${user}    IN    @{EMAILS LIST}
    \  Run Keyword Unless    "${user}"=="${EMAIL OWNER}"    Check Systems Text    ${user}

Should not show systems dropdown with no systems
    [tags]    C41568
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Element Should Not Be Visible    ${SYSTEMS DROPDOWN}

should update owner name in systems list, if it's changed
    Go To    ${url}/account
    Log In    ${EMAIL OWNER}    ${password}    None
    Validate Log In
    Wait Until Elements Are Visible    ${ACCOUNT EMAIL}    ${ACCOUNT FIRST NAME}    ${ACCOUNT LAST NAME}    ${ACCOUNT SAVE}
    #Sleep added here because the account page was populating the first/lastname fields again after Selenium changed it.
    Sleep    1
    Wait Until Textfield Contains    ${ACCOUNT EMAIL}    ${EMAIL OWNER}
    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    ${TEST FIRST NAME}
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
    Reset user owner first/last name