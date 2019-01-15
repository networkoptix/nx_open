*** Settings ***
Resource          ../resource.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers
Force Tags        System

*** Variables ***
${email}           ${EMAIL OWNER}
${password}        ${BASE PASSWORD}
${url}             ${ENV}
${DELETE USER BUTTON ADMIN}               //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL ADMIN}')]/following-sibling::td/a[@ng-click='unshare(user)']/span[text()='${DELETE USER BUTTON TEXT}']
${EDIT USER BUTTON ADMIN}                 //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL ADMIN}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'${EDIT USER BUTTON TEXT}')]/..
${ADMIN USER IN LIST}                     //tr[@ng-repeat='user in system.users']//td[contains(text(), '${EMAIL ADMIN}')]
${share dialogue}

*** Keywords ***
Log in to Auto Tests System
    [arguments]    ${email}
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Log In    ${email}    ${password}    None
    Validate Log In
    Run Keyword If    '${email}' == '${EMAIL OWNER}'    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}    //div[@ng-if='gettingSystem.success']//h2[@class='card-title']
    Run Keyword If    '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}    //div[@ng-if='gettingSystem.success']
    Run Keyword Unless    '${email}' == '${EMAIL OWNER}' or '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${OPEN IN NX BUTTON}

Share with Adminstrator
    [arguments]    ${random email}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Button    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}/following-sibling::div/button/span[text()='${ADMIN TEXT}']
    Click Button    ${SHARE PERMISSIONS DROPDOWN}/following-sibling::div/button/span[text()='${ADMIN TEXT}']/..
    Click Button    ${SHARE BUTTON MODAL}

Check Log In
    Log In    ${EMAIL UNREGISTERED}    ${password}
    Check For Alert    ${ACCOUNT DOES NOT EXIST}
    Log In    ${email}    ${password}    None
    Validate Log In

Check Special Hint
    [arguments]    ${type}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Button    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE MODAL}//nx-permissions-select//li//span[text()='${type}']
    Click Link    ${SHARE MODAL}//nx-permissions-select//li//span[text()='${type}']/..
    ${type}    Convert To Uppercase    ${type}
    Run Keyword If    "${type}"=="${ADMIN TEXT}"          Wait Until Element Contains    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT ADMINISTRATOR}
    ...    ELSE IF    "${type}"=="${ADV VIEWER TEXT}"     Wait Until Element Contains    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT ADVANCED VIEWER}
    ...    ELSE IF    "${type}"=="${VIEWER TEXT}"         Wait Until Element Contains    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT VIEWER}
    ...    ELSE IF    "${type}"=="${LIVE VIEWER TEXT}"    Wait Until Element Contains    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT LIVE VIEWER}
    ...    ELSE IF    "${type}"=="${CUSTOM TEXT}"         Wait Until Element Contains    ${SHARE PERMISSIONS HINT}    ${SHARE PERMISSIONS HINT CUSTOM}

Restart
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out

Reset DB and Open New Browser On Failure
    Close Browser
    Clean up random emails
    Clean up email noperm
    Open Browser and go to URL    ${url}

*** Test Cases ***
Share button - opens dialog
    [tags]    C41888
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}
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
    [tags]    C41888
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

Sharing roles are ordered: more access is on top of the list with options
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE MODAL}//nx-permissions-select//li//span[text()='${ADMIN TEXT}']/../../following-sibling::li/a/span[text()="${ADV VIEWER TEXT}"]/../../following-sibling::li/a/span[text()="${VIEWER TEXT}"]/../../following-sibling::li/a/span[text()="${LIVE VIEWER TEXT}"]/../../following-sibling::li/a/span[text()="Client Custom"]/../../following-sibling::li/a/span[text()="${CUSTOM TEXT}"]
    Click Button    ${SHARE CLOSE}
    Wait Until Page Does Not Contain Element    ${SHARE MODAL}

When user selects role - special hint appears
    [tags]    C41901
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    :FOR    ${type}    IN    @{USER TYPE LIST}
    \  Run Keyword Unless    "${type}"=="${OWNER TEXT}"    Check Special Hint    ${type}
    Click Button    ${SHARE CANCEL}

Sharing works
    Log in to Auto Tests System    ${email}
    ${random email}    Get Random Email    ${BASE EMAIL}
    Share To    ${random email}    ${ADMIN TEXT}
    Check User Permissions    ${random email}    ${ADMIN TEXT}
    Remove User Permissions    ${random email}

displays pencil and cross links for each user only on hover
    Maximize Browser Window
    ${random email}    Get Random Email    ${BASE EMAIL}
    Maximize Browser Window
    Log in to Auto Tests System    ${email}
    Share To    ${random email}    ${VIEWER TEXT}
    Element Should Not Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span[contains(text(),'${DELETE USER BUTTON TEXT}')]
    Element Should Not Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'${EDIT USER BUTTON TEXT}')]/..
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span[contains(text(),'${DELETE USER BUTTON TEXT}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'${EDIT USER BUTTON TEXT}')]/..
    Remove User Permissions    ${random email}

admin cannot delete or edit self
    [tags]    C41904
    Log in to Auto Tests System    ${EMAIL ADMIN}
    Element Should Not Be Visible    ${DELETE USER BUTTON ADMIN}
    Element Should Not Be Visible    ${EDIT USER BUTTON ADMIN}
    Wait Until Element Is Visible    ${ADMIN USER IN LIST}
    Mouse Over    ${ADMIN USER IN LIST}
    Element Should Not Be Visible    ${DELETE USER BUTTON ADMIN}
    Element Should Not Be Visible    ${EDIT USER BUTTON ADMIN}

admin cannot delete or edit other admins
    [tags]    C41905
    Go To    ${url}/register
    ${random email}    Get Random Email    ${BASE EMAIL}
    Register    mark    harmill    ${random email}    ${password}
    Activate    ${random email}
    Log in to Auto Tests System    ${email}
    Share To    ${random email}    ${ADMIN TEXT}
    Log Out
    Validate Log Out
    Log in to Auto Tests System    ${random email}
    Element Should Not Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span[text()='${DELETE USER BUTTON TEXT}']
    Element Should Not Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'${EDIT USER BUTTON TEXT}')]/..
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]
    Element Should Not Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span[text()='${DELETE USER BUTTON TEXT}']
    Element Should Not Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'${EDIT USER BUTTON TEXT}')]/..
    Log Out
    Validate Log Out
    Log in to Auto Tests System    ${email}
    Remove User Permissions    ${random email}

admin cannot invite another admin
    [tags]    C41905
    Log in to Auto Tests System    ${EMAIL ADMIN}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Sleep    2
    Click Button    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE MODAL}//nx-permissions-select//li//span[text()='${VIEWER TEXT}']
    Element Should Not Be Visible    ${SHARE MODAL}//nx-permissions-select//li//span[text()='${ADMIN TEXT']
    Click Button    ${SHARE PERMISSIONS DROPDOWN}
    Click Button    ${SHARE CANCEL}

Edit permission works
    [tags]    C41900
    ${random email}    Get Random Email    ${BASE EMAIL}
    Maximize Browser Window
    Log in to Auto Tests System    ${email}
    Share To    ${random email}    ${ADMIN TEXT}
    Edit User Permissions In Systems    ${random email}    ${CUSTOM TEXT}
    Check User Permissions    ${random email}    ${CUSTOM TEXT}
    Edit User Permissions In Systems    ${random email}    ${ADMIN TEXT}
    Check User Permissions    ${random email}    ${ADMIN TEXT}
    Remove User Permissions    ${random email}

Delete user works
    [tags]    email    C41903
    Go To    ${url}/register
    ${random email}    Get Random Email    ${BASE EMAIL}
    Register    mark    harmill    ${random email}    ${password}
    Activate    ${random email}
    Log in to Auto Tests System    ${email}
    Share To    ${random email}    ${ADMIN TEXT}
    Check User Permissions    ${random email}    ${CUSTOM TEXT}
    Log Out
    Validate Log Out
    Log in to Auto Tests System    ${random email}
    Log Out
    Validate Log Out
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span['&nbsp&nbspDelete']
    Click Element    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${random email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span['&nbsp&nbspDelete']
    Wait Until Element Is Visible    ${DELETE USER CANCEL BUTTON}
    Click Button    ${DELETE USER CANCEL BUTTON}
    Remove User Permissions    ${random email}
    Log Out
    Validate Log Out
    Log In    ${random email}    ${password}
    Wait Until Element Is Visible    ${YOU HAVE NO SYSTEMS}

Share with registered user - sends him notification
    [tags]    email    C41888
    #log in as noperm to check language and change its language to the current testing language
    #otherwise it may receive the notification in another language and fail the email subject comparison
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Sleep    1
    Log Out
    Validate Log Out
    Log in to Auto Tests System    ${email}
    Verify In System    Auto Tests
    Share To    ${EMAIL NO PERM}    ${ADMIN TEXT}
    Check User Permissions    ${EMAIL NOPERM}    ${ADMIN TEXT}
    Open Mailbox    host=${BASE HOST}    password=${BASE EMAIL PASSWORD}    port=${BASE PORT}    user=${BASE EMAIL}    is_secure=True
    ${INVITED TO SYSTEM EMAIL SUBJECT}    Replace String    ${INVITED TO SYSTEM EMAIL SUBJECT}    {{message.system_name}}    ${AUTO TESTS}
    ${email}    Wait For Email    recipient=${EMAIL NOPERM}    timeout=120
    Check Email Subject    ${email}    ${INVITED TO SYSTEM EMAIL SUBJECT}    ${BASE EMAIL}    ${BASE EMAIL PASSWORD}    ${BASE HOST}    ${BASE PORT}
    Remove User Permissions    ${EMAIL NOPERM}

Share with unregistered user - brings them to registration page with code with correct email locked
    [tags]    email    C41889
    ${random email}    Get Random Email    ${BASE EMAIL}
    Log in to Auto Tests System    ${email}
    Verify In System    Auto Tests
    Share To    ${random email}    ${ADMIN TEXT}
    Check User Permissions    ${random email}    ${CUSTOM TEXT}
    Log Out
    Validate Log Out
    ${link}    Get Email Link    ${random email}    register
    Go To    ${link}
    Register    ${TEST FIRST NAME}    ${TEST LAST NAME}    ${random email}    ${password}
    Validate Log In

Sharing system with a user who is already in the list updates their permissions
    [tags]    C41892
    ${random email}    Get Random Email    ${BASE EMAIL}
    Log in to Auto Tests System    ${email}
    Verify In System    Auto Tests
    Share To    ${random email}    ${ADMIN TEXT}
    Check User Permissions    ${random email}    ${ADMIN TEXT}
    Share To    ${random email}    ${VIEWER TEXT}
    Check User Permissions    ${random email}    ${VIEWER TEXT}
    Remove User Permissions    ${random email}

Check share email for registered user
    [tags]    C47297
    #log in as noperm to check language and change its language to the current testing language
    #otherwise it may receive the notification in another language and fail the email subject comparison
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Sleep    1
    Log Out
    Validate Log Out
    Log in to Auto Tests System    ${email}
    Verify In System    Auto Tests
    Share To    ${EMAIL NO PERM}    ${ADMIN TEXT}
    Check User Permissions    ${EMAIL NOPERM}    ${ADMIN TEXT}
    Open Mailbox    host=${BASE HOST}    password=${BASE EMAIL PASSWORD}    port=${BASE PORT}    user=${BASE EMAIL}    is_secure=True
    ${INVITED TO SYSTEM EMAIL SUBJECT}    Replace String    ${INVITED TO SYSTEM EMAIL SUBJECT}    {{message.system_name}}    ${AUTO TESTS}
    ${email}    Wait For Email    recipient=${EMAIL NOPERM}    timeout=120
    ${email text}    Get Email Body    ${email}
    Check Email Subject    ${email}    ${INVITED TO SYSTEM EMAIL SUBJECT}    ${BASE EMAIL}    ${BASE EMAIL PASSWORD}    ${BASE HOST}    ${BASE PORT}
    Check Email Button    ${email text}    ${ENV}    ${THEME COLOR}
    ${links}    Get Links From Email    ${email}
    @{expected links}    Set Variable    ${SUPPORT URL}    ${WEBSITE URL}    ${ENV}    ${ENV}/systems/${AUTO_TESTS SYSTEM ID}    mailto:${OWNER EMAIL}
    : FOR    ${link}  IN  @{links}
    \    check in list    ${expected links}    ${link}
    Delete Email    ${email}
    Close Mailbox
    Remove User Permissions    ${EMAIL NOPERM}

User with client custom settings has access to system
    Log in to Auto Tests System    ${EMAIL CLIENT CUSTOM}
    Location Should Be    ${url}/systems/${AUTO_TESTS SYSTEM ID}
    Verify In System    ${AUTO TESTS}

User can be invited with client custom permissions
    Log in to Auto Tests System    ${EMAIL OWNER}
    ${random email}    Get Random Email    ${BASE EMAIL}
    Share To    ${random email}    Client Custom
    Check User Permissions    ${random email}    Client Custom
    Sleep    2
    Remove User Permissions    ${random email}