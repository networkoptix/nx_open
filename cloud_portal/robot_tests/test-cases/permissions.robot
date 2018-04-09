*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Teardown     Close Browser
Suite Teardown    Run Keyword If Any Tests Failed    Permissions Failure
Force Tags        system

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

Permissions Failure
    Clean up random emails
    Clean up email noperm

*** Test Cases ***
Share button - opens dialog
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    Wait Until Elements Are Visible    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Element Is Visible    ${SHARE MODAL}

Sharing link /systems/{system_id}/share - opens dialog
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    ${location}    Get Location
    Go To    ${location}/share
    Wait Until Element Is Visible    ${SHARE MODAL}

Sharing link for anonymous - first ask login, then show share dialog
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    ${location}    Get Location
    Log Out
    Go To    ${location}/share
    Log In    ${email}    ${password}    button=None
    Wait Until Element Is Visible    ${SHARE MODAL}

After closing dialog, called by link - clear link
    Open Browser and go to URL    ${url}
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
    Wait Until Elements Are Visible    ${SHARE MODAL}    //div[@uib-modal-window="modal-window"]
    Click Element At Coordinates    //div[@uib-modal-window="modal-window"]    50    50
    Wait Until Page Does Not Contain Element    ${SHARE MODAL}
    Location Should Be    ${location}

Sharing roles are ordered: more access is on top of the list with options
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Element Text Should Be    ${SHARE PERMISSIONS DROPDOWN}    ${ADMIN TEXT}\n${ADV VIEWER TEXT}\n${VIEWER TEXT}\n${LIVE VIEWER TEXT}\n${CUSTOM TEXT}

When user selects role - special hint appears
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    Click Button    ${SHARE BUTTON SYSTEMS}
#Adminstrator check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS ADMINISTRATOR}
    Click Element    ${SHARE PERMISSIONS ADMINISTRATOR}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT ADMIN}
#Advanced Viewer Check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS ADVANCED VIEWER}
    Click Element    ${SHARE PERMISSIONS ADVANCED VIEWER}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT ADV VIEWER}
#Viewer Check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS VIEWER}
    Click Element    ${SHARE PERMISSIONS VIEWER}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT VIEWER}
#Live Viewer Check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS LIVE VIEWER}
    Click Element    ${SHARE PERMISSIONS LIVE VIEWER}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT LIVE VIEWER}
#Custom Check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS CUSTOM}
    Click Element    ${SHARE PERMISSIONS CUSTOM}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT CUSTOM}

Sharing works
    Open Browser and go to URL    ${url}
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
    Open Browser and go to URL    ${url}
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
    Open Browser and go to URL    ${url}
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
    Open Browser and go to URL    ${url}/register
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
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Log Out
    Validate Log Out
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
    ${INVITED TO SYSTEM EMAIL SUBJECT}    Replace String    ${INVITED TO SYSTEM EMAIL SUBJECT}    {{message.sharer_name}}    ${TEST FIRST NAME} ${TEST LAST NAME}
    ${email}    Wait For Email    recipient=${EMAIL NOPERM}    timeout=120
    Check Email Subject    ${email}    ${INVITED TO SYSTEM EMAIL SUBJECT}    ${BASE EMAIL}    ${BASE EMAIL PASSWORD}    ${BASE HOST}    ${BASE PORT}
    Remove User Permissions    ${EMAIL NOPERM}
