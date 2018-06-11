*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers
Force Tags        system

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${ENV}

*** Keywords ***
Log in to Auto Tests System
    [arguments]    ${email}
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Log In    ${email}    ${password}    None
    Validate Log In
    Run Keyword If    '${email}' == '${EMAIL OWNER}'    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword If    '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword Unless    '${email}' == '${EMAIL OWNER}' or '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${OPEN IN NX BUTTON}

Check System Text
    [arguments]    ${user}
    Log Out
    Validate Log Out
    Log in to Auto Tests System    ${user}
    Wait Until Elements Are Visible    //h2[.='${OWNER TEXT}']    //a[.='${EMAIL OWNER}']
    Wait Until Element Is Not Visible    //h2[.='${YOUR SYSTEM TEXT}']

Reset DB and Open New Browser On Failure
    Close Browser
    Reset System Names
    Make sure notowner is in the system
    Open Browser and go to URL    ${url}

Restart
    ${status}    Run Keyword And Return Status    Validate Log In
    Run Keyword If    ${status}    Log Out
    Go To    ${url}

*** Test Cases ***
systems dropdown should allow you to go back to the systems page
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${SYSTEMS DROPDOWN}
    Click Link    ${SYSTEMS DROPDOWN}
    Wait Until Element Is Visible    ${ALL SYSTEMS}
    Click Link    ${ALL SYSTEMS}
    Location Should Be    ${url}/systems

should confirm, if owner deletes system (You are going to disconnect your system from cloud)
    Log in to Auto Tests System    ${EMAIL OWNER}
    Click Button    ${DISCONNECT FROM NX}
    Wait Until Elements Are Visible    ${DISCONNECT FORM}    ${DISCONNECT FORM HEADER}
    Click Button    ${DISCONNECT FORM CANCEL}
    Wait Until Page Does Not Contain Element    ${DELETE USER MODAL}

should confirm, if not owner deletes system (You will loose access to this system)
    Log In To Auto Tests System    ${EMAIL NOT OWNER}
    Validate Log In
    Wait Until Element Is Visible    ${DISCONNECT FROM MY ACCOUNT}
    Click Button    ${DISCONNECT FROM MY ACCOUNT}
    Wait Until Element Is Visible    ${DISCONNECT MODAL WARNING}
    Click Button    ${DISCONNECT MODAL CANCEL}
    Wait Until Page Does Not Contain Element    ${DELETE USER MODAL}

Cancel should cancel disconnection and disconnect should remove it when not owner
    Log In To Auto Tests System    ${EMAIL NOT OWNER}
    Validate Log In
    Wait Until Element Is Visible    ${DISCONNECT FROM MY ACCOUNT}
    Click Button    ${DISCONNECT FROM MY ACCOUNT}
    Wait Until Elements Are Visible    ${DISCONNECT MODAL WARNING}    ${DISCONNECT MODAL CANCEL}
    Click Button    ${DISCONNECT MODAL CANCEL}
    Wait Until Element Is Not Visible    ${DISCONNECT MODAL WARNING}
    Wait Until Page Does Not Contain Element    //div[@modal-render='true']
    Wait Until Element Is Visible    ${DISCONNECT FROM MY ACCOUNT}
    Click Button    ${DISCONNECT FROM MY ACCOUNT}
    Wait Until Elements Are Visible    ${DISCONNECT MODAL WARNING}    ${DISCONNECT MODAL DISCONNECT BUTTON}
    Click Button    ${DISCONNECT MODAL DISCONNECT BUTTON}
    Wait Until Element Is Visible    ${YOU HAVE NO SYSTEMS}
    Log Out
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${EMAIL NOT OWNER}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}

has Share button and user list visible for admin and owner
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${USERS LIST}
    Log Out
    Log in to Auto Tests System    ${EMAIL ADMIN}
    Wait Until Element Is Visible    ${USERS LIST}

does not show Share button, Rename button, or user list to viewer, advanced viewer, live viewer
#This allows the expected error to not run the close browser action
    Log in to Auto Tests System    ${EMAIL VIEWER}
    Register Keyword To Run On Failure    NONE
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${USERS LIST}
    Register Keyword To Run On Failure    Failure Tasks
    Element Should Not Be Visible    ${SHARE BUTTON SYSTEMS}
    Log Out
    Log in to Auto Tests System    ${EMAIL ADV VIEWER}
    Register Keyword To Run On Failure    NONE
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${USERS LIST}
    Register Keyword To Run On Failure    Failure Tasks
    Element Should Not Be Visible    ${SHARE BUTTON SYSTEMS}
    Log Out
    Log in to Auto Tests System    ${EMAIL LIVE VIEWER}
    Register Keyword To Run On Failure    NONE
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${USERS LIST}
    Register Keyword To Run On Failure    Failure Tasks
    Element Should Not Be Visible    ${SHARE BUTTON SYSTEMS}

does not display edit and remove for owner row
    Log in to Auto Tests System    ${EMAIL ADMIN}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL OWNER}')]
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL OWNER}')]
    Element Should Not Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL OWNER}')]/following-sibling::td/a[@ng-click='unshare(user)']/span['&nbsp&nbspDelete']
    Element Should Not Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL OWNER}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'Edit')]/..

always displays owner on the top of the table
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${FIRST USER OWNER}

contains user emails and names
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL ADMIN}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${ADMIN FIRST NAME} ${ADMIN LAST NAME}')]

rename button opens dialog; cancel closes without rename; save renames system
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}
    Click Button    ${RENAME CANCEL}
    Wait Until Page Does Not Contain Element    //div[@modal-render='true']
    Verify In System    Auto Tests
    Wait Until Elements Are Visible    ${RENAME SYSTEM}    ${OPEN IN NX BUTTON}    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}    ${RENAME INPUT}
    Clear Element Text    ${RENAME INPUT}
    Input Text    ${RENAME INPUT}    Auto Tests Rename
    Click Button    ${RENAME SAVE}
    Check For Alert    ${SYSTEM NAME SAVED}
    Verify In System    Auto Tests Rename
    Wait Until Elements Are Visible    ${RENAME SYSTEM}    ${OPEN IN NX BUTTON}    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}    ${RENAME INPUT}
    Clear Element Text    ${RENAME INPUT}
    Input Text    ${RENAME INPUT}    Auto Tests
    Click Button    ${RENAME SAVE}
    Check For Alert    ${SYSTEM NAME SAVED}
    Verify In System    Auto Tests

should open System page by link to not authorized user and redirect to homepage, if he does not log in
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Click Button    ${LOG IN CLOSE BUTTON}
    Wait Until Element Is Visible    ${JUMBOTRON}

should open System page by link to not authorized user and show it, after owner logs in
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Log In    ${EMAIL OWNER}   ${password}    None
    Verify In System    Auto Tests

should open System page by link to user without permission and show alert (System info is unavailable: You have no access to this system)
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Wait Until Element Is Visible    ${SYSTEM NO ACCESS}

should open System page by link not authorized user, and show alert if logs in and has no permission
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Log In    ${EMAIL NOPERM}   ${password}    None
    Wait Until Element Is Visible    ${SYSTEM NO ACCESS}

should display same user data as user provided during registration (stress to cyrillic)
    [tags]    email
#create user
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    ${CYRILLIC TEXT}    ${CYRILLIC TEXT}    ${email}    ${password}
    Activate    ${email}
#share system with new user
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE PERMISSIONS DROPDOWN}
    Input Text    ${SHARE EMAIL}    ${email}
    Select From List By Label    ${SHARE PERMISSIONS DROPDOWN}    ${ADMIN TEXT}
    Wait Until Element Is Visible    ${SHARE BUTTON MODAL}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Log Out

#verify user was added with appropriate name
    Log In    ${email}    ${password}
    Wait Until Element Is Visible    //td[contains(text(),'${CYRILLIC TEXT} ${CYRILLIC TEXT}')]

#remove new user from system
    Log Out
    Log in to Auto Tests System    ${EMAIL OWNER}
    Remove User Permissions    ${email}

should display same user data as showed in user account (stress to cyrillic)
    [tags]    email
#create user
    ${email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Activate    ${email}
#share system with new user
    Log in to Auto Tests System    ${EMAIL OWNER}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE PERMISSIONS DROPDOWN}
    Input Text    ${SHARE EMAIL}    ${email}
    Select From List By Label    ${SHARE PERMISSIONS DROPDOWN}    ${ADMIN TEXT}
    Wait Until Element Is Visible    ${SHARE BUTTON MODAL}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Log Out

    Go To    ${url}/account
    Log In    ${email}    ${password}    None
    Validate Log In
    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    mark
    Wait Until Textfield Contains    ${ACCOUNT LAST NAME}    hamill
    Clear Element Text    ${ACCOUNT FIRST NAME}
    Input Text    ${ACCOUNT FIRST NAME}    ${CYRILLIC TEXT}
    Clear Element Text    ${ACCOUNT LAST NAME}
    Input Text    ${ACCOUNT LAST NAME}    ${CYRILLIC TEXT}
    sleep    .15
    Wait Until Element Is Visible    ${ACCOUNT SAVE}
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Wait Until Element Is Visible    //td[contains(text(),'${CYRILLIC TEXT} ${CYRILLIC TEXT}')]

    #remove new user from system
    Log Out
    Log in to Auto Tests System    ${EMAIL OWNER}
    Remove User Permissions    ${email}

should show "your system" for owner and "owner's name" for non-owners
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    //h2[.='${YOUR SYSTEM TEXT}']
    Wait Until Element Is Not Visible    //h2[.='${OWNER TEXT}']
    :FOR    ${user}    IN    @{EMAILS LIST}
    \  Run Keyword Unless    "${user}"=="${EMAIL OWNER}"    Check System Text    ${user}