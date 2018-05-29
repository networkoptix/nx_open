*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Setup        Restart
#Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers
Force Tags        System

*** Variables ***
${email}           ${EMAIL OWNER}
${password}        ${BASE PASSWORD}
${url}             ${ENV}
${share dialogue}

*** Keywords ***
Log in to Auto Tests System
    [arguments]    ${email}
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Log In    ${email}    ${password}    None
    Validate Log In
    Run Keyword If    '${email}' == '${EMAIL OWNER}'    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword If    '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword Unless    '${email}' == '${EMAIL OWNER}' or '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${OPEN IN NX BUTTON}

Check Log In
    Log In    ${EMAIL UNREGISTERED}    ${password}
    Check For Alert    ${ACCOUNT DOES NOT EXIST}
    Log In    ${email}    ${password}    None
    Validate Log In

Restart
    ${status}    Run Keyword And Return Status    Validate Log In
    Run Keyword If    ${status}    Log Out

Reset DB and Open New Browser On Failure
    Close Browser
    Clean up random emails
    Clean up email noperm
    Open Browser and go to URL    ${url}

*** Test Cases ***
Share button - opens dialog
    Log in to Auto Tests System    ${email}
    Wait Until Elements Are Visible    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Element Is Visible    ${SHARE MODAL}
    Click Button    ${SHARE CLOSE}
    Wait Until Page Does Not Contain Element    ${SHARE MODAL}

Sharing link /systems/{system_id}/share - opens dialog
    Log in to Auto Tests System    ${email}
    ${location}    Get Location
    Go To    ${location}/share
    Wait Until Element Is Visible    ${SHARE MODAL}
    Click Button    ${SHARE CLOSE}
    Wait Until Page Does Not Contain Element    ${SHARE MODAL}

Sharing link for anonymous - first ask login, then show share dialog
    Log in to Auto Tests System    ${email}
    ${location}    Get Location
    Log Out
    Go To    ${location}/share
    Log In    ${email}    ${password}    button=None
    Wait Until Element Is Visible    ${SHARE MODAL}
    Click Button    ${SHARE CLOSE}
    Wait Until Page Does Not Contain Element    ${SHARE MODAL}

After closing dialog, called by link - clear link
    Set Window Size    1920    1080
    Log in to Auto Tests System    ${email}
    ${location}    Get Location

#Check Cancel Button
    Go To    ${location}/share
    Wait Until Elements Are Visible    ${SHARE MODAL}    ${SHARE CANCEL}
    Click Button    ${SHARE CANCEL}
    Wait Until Element Is Not Visible    ${SHARE MODAL}
    Location Should Be    ${location}

#Check 'X' Button
    Go To    ${location}/share
    Wait Until Elements Are Visible    ${SHARE MODAL}    ${SHARE CLOSE}
    Wait Until Element Is Visible    ${SHARE CLOSE}
    Click Button    ${SHARE CLOSE}
    Wait Until Element Is Not Visible    ${SHARE MODAL}
    Location Should Be    ${location}

#Check Background Click
    Go To    ${location}/share
    Wait Until Elements Are Visible    ${SHARE MODAL}    ${BACKDROP}
    Click Element At Coordinates    ${BACKDROP}    50    50
    Wait Until Page Does Not Contain Element    ${SHARE MODAL}
    Location Should Be    ${location}

Sharing roles are ordered: more access is on top of the list with options
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Sleep    2
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE MODAL}//nx-permissions-select//button/span[text()='${ADMIN TEXT}']/../following-sibling::button/span[text()='${ADV VIEWER TEXT}']/../following-sibling::button/span[text()='${VIEWER TEXT}']/../following-sibling::button/span[text()='${LIVE VIEWER TEXT}']/../following-sibling::button/span[text()='${CUSTOM TEXT}']
    Click Button    ${SHARE CLOSE}
    Wait Until Page Does Not Contain Element    ${SHARE MODAL}

When user selects role - special hint appears
    Log in to Auto Tests System    ${email}
    Click Button    ${SHARE BUTTON SYSTEMS}
#Adminstrator check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Select From List By Label    ${SHARE PERMISSIONS DROPDOWN}    ${ADMIN TEXT}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT ADMIN}
#Advanced Viewer Check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Select From List By Label    ${SHARE PERMISSIONS DROPDOWN}    ${ADV VIEWER TEXT}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT ADV VIEWER}
#Viewer Check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Select From List By Label    ${SHARE PERMISSIONS DROPDOWN}    ${VIEWER TEXT}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT VIEWER}
#Live Viewer Check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Select From List By Label    ${SHARE PERMISSIONS DROPDOWN}    ${LIVE VIEWER TEXT}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT LIVE VIEWER}
#Custom Check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Select From List By Label    ${SHARE PERMISSIONS DROPDOWN}    ${CUSTOM TEXT}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT CUSTOM}
    Click Button    ${SHARE CLOSE}
    Wait Until Page Does Not Contain Element    ${SHARE MODAL}

Sharing works
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email}    Get Random Email    ${BASE EMAIL}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Check User Permissions    ${random email}    ${CUSTOM TEXT}
    Remove User Permissions    ${random email}

displays pencil and cross links for each user only on hover
    Maximize Browser Window
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email}    Get Random Email    ${BASE EMAIL}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Element Should Not Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span['&nbsp&nbspDelete']
    Element Should Not Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'${EDIT USER BUTTON TEXT}')]/..
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span['&nbsp&nbspDelete']
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'${EDIT USER BUTTON TEXT}')]/..
    Remove User Permissions    ${random email}

Edit permission works
    Maximize Browser Window
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email}    Get Random Email    ${BASE EMAIL}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Edit User Permissions In Systems    ${random email}    ${VIEWER TEXT}
    Check User Permissions    ${random email}    ${VIEWER TEXT}
    Edit User Permissions In Systems    ${random email}    ${CUSTOM TEXT}
    Check User Permissions    ${random email}    ${CUSTOM TEXT}
    Remove User Permissions    ${random email}

Delete user works
    [tags]    email
    Go To    ${url}/register
    ${random email}    Get Random Email    ${BASE EMAIL}
    Register    mark    harmill    ${random email}    ${password}
    Activate    ${random email}
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Check User Permissions    ${random email}    ${CUSTOM TEXT}
    Log Out
    Validate Log Out
    Log in to Auto Tests System    ${random email}
    Log Out
    Validate Log Out
    Log in to Auto Tests System    ${email}
    Remove User Permissions    ${random email}
    Log Out
    Validate Log Out
    Log In    ${random email}    ${password}
    Wait Until Element Is Visible    ${YOU HAVE NO SYSTEMS}

Share with registered user - sends him notification
    [tags]    email
    Log in to Auto Tests System    ${email}
    Verify In System    Auto Tests
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${EMAIL NOPERM}
#This sleep is because it throws an error and is clickable too soon.
    Sleep    .5
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Check User Permissions    ${EMAIL NOPERM}    ${CUSTOM TEXT}
    Open Mailbox    host=${BASE HOST}    password=${BASE EMAIL PASSWORD}    port=${BASE PORT}    user=${BASE EMAIL}    is_secure=True
    ${INVITED TO SYSTEM EMAIL SUBJECT}    Replace String    ${INVITED TO SYSTEM EMAIL SUBJECT}    {{message.system_name}}    ${AUTO TESTS}
    ${email}    Wait For Email    recipient=${EMAIL NOPERM}    timeout=120
    Check Email Subject    ${email}    ${INVITED TO SYSTEM EMAIL SUBJECT}    ${BASE EMAIL}    ${BASE EMAIL PASSWORD}    ${BASE HOST}    ${BASE PORT}
    Remove User Permissions    ${EMAIL NOPERM}

Share with unregistered user - brings them to registration page with code with correct email locked
    [tags]    email
    Log in to Auto Tests System    ${email}
    Verify In System    Auto Tests
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    ${random email}    Get Random Email    ${BASE EMAIL}
    Input Text    ${SHARE EMAIL}    ${random email}
    Sleep    .5
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Check User Permissions    ${random email}    ${CUSTOM TEXT}
    Log Out
    Validate Log Out
    ${link}    Get Email Link    ${random email}    register
    Go To    ${link}
    Register    ${TEST FIRST NAME}    ${TEST LAST NAME}    ${random email}    ${password}
    Validate Log In