*** Settings ***
Resource          ../resource.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers
Force Tags        system

*** Variables ***
${email}       ${EMAIL OWNER}
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
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}

*** Test Cases ***
systems dropdown should allow you to go back to the systems page
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${SYSTEMS DROPDOWN}
    Click Button    ${SYSTEMS DROPDOWN}
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
    [tags]    C41884
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
    ${SYSYEM DELETED FROM ACCOUNT}    Replace String    ${SYSYEM DELETED FROM ACCOUNT}    {{system_name}}    ${AUTO TESTS}
    Check For Alert    ${SYSYEM DELETED FROM ACCOUNT}
    Wait Until Element Is Visible    ${YOU HAVE NO SYSTEMS}
    Log Out
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${EMAIL NOT OWNER}
    Wait Until Element Contains    ${SHARE PERMISSIONS DROPDOWN}    ${VIEWER TEXT}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}

correct items are shown for owner
    [tags]    C41560
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${USERS LIST}
#    Wait Until Elements Are Visible    //h2[.='${OWNER TEXT}']
    Wait Until Elements Are Visible    //h2[.='${YOUR SYSTEM TEXT}']    ${RENAME SYSTEM}    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}

correct items are shown for admin
    [tags]    C41561
    Log in to Auto Tests System    ${EMAIL ADMIN}
    Wait Until Elements Are Visible    ${RENAME SYSTEM}    ${DISCONNECT FROM MY ACCOUNT}    ${SHARE BUTTON SYSTEMS}    ${OWNER NAME}    ${OWNER EMAIL}

correct items are shown for advanced viewer and below
    [tags]    C41562
    ${users}         Set Variable    ${EMAIL ADVVIEWER}    ${EMAIL VIEWER}    ${EMAIL LIVEVIEWER}    ${EMAIL CUSTOM}
    ${users text}    Set Variable    ${ADV VIEWER TEXT}    ${VIEWER TEXT}     ${LIVE VIEWER TEXT}    ${CUSTOM TEXT}
    :FOR    ${user}  ${text}  IN ZIP  ${users}  ${users text}
    \    Log in to Auto Tests System    ${user}
    \    Log     ${text}
    \    Wait Until Elements Are Visible    ${OWNER NAME}    ${OWNER EMAIL}    ${YOUR PERMISSIONS}    ${YOUR PERMISSIONS}/b[contains(text(),"${text}")]
    \    Element Should Not Be Visible    ${RENAME SYSTEM}
    \    Element Should Not Be Visible    ${SHARE BUTTON SYSTEMS}
    \    Element Should Not Be Visible    ${USERS LIST}
    \    Log Out
    \    Validate Log Out

does not display edit and remove for owner row
    Log in to Auto Tests System    ${EMAIL ADMIN}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL OWNER}')]
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL OWNER}')]
    Element Should Not Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL OWNER}')]/following-sibling::td/a[@ng-click='unshare(user)']/span[text()='${DELETE USER BUTTON TEXT}']
    Element Should Not Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL OWNER}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'${EDIT USER BUTTON TEXT}')]/..

always displays owner on the top of the table
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${FIRST USER OWNER}

contains user emails and names
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL ADMIN}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${ADMIN FIRST NAME} ${ADMIN LAST NAME}')]

rename button opens dialog and clicking cancel closes rename dialog without rename
    [tags]    C41880
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}
    Click Button    ${RENAME CANCEL}
    Wait Until Page Does Not Contain Element    //div[@uib-modal-backdrop="modal-backdrop"]
    Verify In System    Auto Tests

clicking 'X' closes rename dialog without rename
    [tags]    C41880
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}    ${RENAME X BUTTON}
    Wait Until Textfield Contains    ${RENAME INPUT}    ${AUTO TESTS}
    Click Button    ${RENAME X BUTTON}
    Wait Until Page Does Not Contain Element    ${BACKDROP}
    Verify In System    Auto Tests

clicking save with no input in rename dialoge throws error
    [tags]    C41880
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME SYSTEM}    ${OPEN IN NX BUTTON}    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}    ${RENAME INPUT}
    sleep    2
    Input Text    ${RENAME INPUT}    ${SPACE}
    Press Key    ${RENAME INPUT}    ${BACKSPACE}
    Click Button    ${RENAME SAVE}
    Wait Until Elements Are Visible    ${RENAME INPUT WITH ERROR}    ${SYSTEM NAME IS REQUIRED}
    Click Button    ${RENAME CANCEL}

clicking save in rename dialog renames system
    [tags]    C41880
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${RENAME SYSTEM}
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
    Log In    ${EMAIL NOPERM}    ${password}    None
    Wait Until Element Is Visible    ${SYSTEM NO ACCESS}

should display same user data as user provided during registration (stress to cyrillic)
    [tags]    email
#create user
    ${random email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    ${CYRILLIC TEXT}    ${CYRILLIC TEXT}    ${random email}    ${password}
    Activate    ${random email}
#share system with new user
    Log in to Auto Tests System    ${EMAIL OWNER}
    Share To    ${random email}    ${ADMIN TEXT}
    Log Out

#verify user was added with appropriate name
    Log In    ${random email}    ${password}
    Wait Until Element Is Visible    //td[contains(text(),'${CYRILLIC TEXT} ${CYRILLIC TEXT}')]

#remove new user from system
    Log Out
    Log in to Auto Tests System    ${EMAIL OWNER}
    Remove User Permissions    ${random email}

should display same user data as showed in user account (stress to cyrillic)
    [tags]    email    C41573    C41842
#create user
    ${random email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${random email}    ${password}
    Activate    ${random email}
#share system with new user
    Log in to Auto Tests System    ${EMAIL OWNER}
    Share To    ${random email}    ${VIEWER TEXT}
    Log Out

    Go To    ${url}/account
    Log In    ${random email}    ${password}    None
    Validate Log In
    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    mark
    Wait Until Textfield Contains    ${ACCOUNT LAST NAME}    hamill
    # sometimes the text field refills itself if I don't wait a second
    sleep    1
    Clear Element Text    ${ACCOUNT FIRST NAME}
    Input Text    ${ACCOUNT FIRST NAME}    ${CYRILLIC TEXT}
    Clear Element Text    ${ACCOUNT LAST NAME}
    Input Text    ${ACCOUNT LAST NAME}    ${CYRILLIC TEXT}
    sleep    .15
    Wait Until Element Is Visible    ${ACCOUNT SAVE}
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Log Out
    Validate Log Out

    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    //td[contains(text(),'${CYRILLIC TEXT} ${CYRILLIC TEXT}')]

    #remove new user from system
    Log Out
    Log in to Auto Tests System    ${EMAIL OWNER}
    Remove User Permissions    ${random email}

should show "your system" for owner and "owner's name" for non-owners
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    //h2[.='${YOUR SYSTEM TEXT}']
    Wait Until Element Is Not Visible    //h2[.='${OWNER TEXT}']
    :FOR    ${user}    IN    @{EMAILS LIST}
    \  Run Keyword Unless    "${user}"=="${EMAIL OWNER}"    Check System Text    ${user}