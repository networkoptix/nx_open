*** Settings ***

Library           SeleniumLibrary    screenshot_root_directory=\Screenshots    run_on_failure=Failure Tasks
Library           NoptixImapLibrary/
Library           String
Library           NoptixLibrary/
Resource          variables.robot

*** Keywords ***
Open Browser and go to URL
    [Arguments]    ${url}
    Open Browser    ${ENV}    ${BROWSER}
#    Maximize Browser Window
    Set Selenium Speed    0
    Check Language
    Go To    ${url}

Check Language
    Wait Until Element Is Visible    ${LANGUAGE DROPDOWN}
    Register Keyword To Run On Failure    NONE
    ${status}    ${value}=    Run Keyword And Ignore Error    Wait Until Element Is Visible    ${LANGUAGE DROPDOWN}/span[@lang='${LANGUAGE}']    2
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    "${status}"=="FAIL"    Set Language

Set Language
    Click Button    ${LANGUAGE DROPDOWN}
    Wait Until Element Is Visible    ${LANGUAGE TO SELECT}
    Click Element    ${LANGUAGE TO SELECT}
    Wait Until Element Is Visible    ${LANGUAGE DROPDOWN}/span[@lang='${LANGUAGE}']    5

Log In
    [arguments]    ${email}    ${password}    ${button}=${LOG IN NAV BAR}
    Run Keyword Unless    '''${button}''' == "None"    Wait Until Element Is Visible    ${button}
    Run Keyword Unless    '''${button}''' == "None"    Click Link    ${button}
    Wait Until Elements Are Visible    ${EMAIL INPUT}    ${PASSWORD INPUT}
    Input Text    ${EMAIL INPUT}    ${email}
    Input Text    ${PASSWORD INPUT}    ${password}

    Wait Until Element Is Visible    ${LOG IN BUTTON}
    Click Button    ${LOG IN BUTTON}

Validate Log In
    Wait Until Page Contains Element    ${AUTHORIZED BODY}
    Page Should Contain Element    ${AUTHORIZED BODY}
    Check Language

Log Out
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Click Element    ${ACCOUNT DROPDOWN}
    Wait Until Element Is Visible    ${LOG OUT BUTTON}
    Click Link    ${LOG OUT BUTTON}
    Validate Log Out

Validate Log Out
    Wait Until Element Is Visible    ${ANONYMOUS BODY}

Register
    [arguments]    ${first name}    ${last name}    ${email}    ${password}    ${checked}=true
    Wait Until Elements Are Visible    ${REGISTER FIRST NAME INPUT}    ${REGISTER LAST NAME INPUT}    ${REGISTER EMAIL INPUT}    ${REGISTER PASSWORD INPUT}    ${CREATE ACCOUNT BUTTON}
    Input Text    ${REGISTER FIRST NAME INPUT}    ${first name}
    Input Text    ${REGISTER LAST NAME INPUT}    ${last name}
    Input Text    ${REGISTER EMAIL INPUT}    ${email}
    Input Text    ${REGISTER PASSWORD INPUT}    ${password}
    Run Keyword If    "${checked}"=="false"    Click Element    ${REGISTER SUBSCRIBE CHECKBOX}
    Click Button    ${CREATE ACCOUNT BUTTON}

Validate Register Success
    [arguments]    ${location}=${url}/register/success
    Wait Until Element Is Visible    ${ACCOUNT CREATION SUCCESS}
    Location Should Be    ${location}

Validate Register Email Received
    [arguments]    ${recipient}
    Open Mailbox    host=${BASE HOST}    password=${BASE EMAIL PASSWORD}    port=${BASE PORT}    user=${BASE EMAIL}    is_secure=True
    ${email}    Wait For Email    recipient=${recipient}    timeout=120    status=UNSEEN
    Check Email Subject    ${email}    ${ACTIVATE YOUR ACCOUNT EMAIL SUBJECT}    ${BASE EMAIL}    ${BASE EMAIL PASSWORD}    ${BASE HOST}    ${BASE PORT}
    Should Not Be Equal    ${email}    ${EMPTY}
    Close Mailbox

Get Email Link
    [arguments]    ${recipient}    ${link type}
    Open Mailbox    host=${BASE HOST}    password=${BASE EMAIL PASSWORD}    port=${BASE PORT}    user=${BASE EMAIL}    is_secure=True
    ${email}    Wait For Email    recipient=${recipient}    timeout=120    status=UNSEEN
    Run Keyword If    "${link type}"=="activate"    Check Email Subject    ${email}    ${ACTIVATE YOUR ACCOUNT EMAIL SUBJECT}    ${BASE EMAIL}    ${BASE EMAIL PASSWORD}    ${BASE HOST}    ${BASE PORT}
    Run Keyword If    "${link type}"=="reset"    Check Email Subject    ${email}    ${RESET PASSWORD EMAIL SUBJECT}    ${BASE EMAIL}    ${BASE EMAIL PASSWORD}    ${BASE HOST}    ${BASE PORT}
    ${links}    Get Links From Email    ${email}
    Mark All Emails As Read
    Close Mailbox
    Return From Keyword    ${links}

Activate
    [arguments]    ${email}
    ${link}    Get Email Link    ${email}    activate
    Go To    ${link}
    Wait Until Element Is Visible    ${ACTIVATION SUCCESS}
    Element Should Be Visible    ${ACTIVATION SUCCESS}
    Location Should Be    ${url}/activate/success

Edit User Permissions In Systems
    [arguments]    ${user email}    ${permissions}
    Wait Until Element Is Not Visible    ${SHARE MODAL}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'${EDIT USER BUTTON TEXT}')]/..
    Click Element    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'${EDIT USER BUTTON TEXT}')]/..
    Wait Until Element Is Visible    //form[@name='shareForm']//select[@ng-model='user.role']//option[@label='${permissions}']
    Click Element    //form[@name='shareForm']//select[@ng-model='user.role']//option[@label='${permissions}']
    Wait Until Element Is Visible    ${EDIT PERMISSIONS SAVE}
    Click Element    ${EDIT PERMISSIONS SAVE}
    Check For Alert    ${NEW PERMISSIONS SAVED}

Check User Permissions
    [arguments]    ${user email}    ${permissions}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/span['${permissions}']
    Element Should Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/span['${permissions}']

Remove User Permissions
    [arguments]    ${user email}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span['&nbsp&nbspDelete']
    Click Element    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span['&nbsp&nbspDelete']
    Wait Until Element Is Visible    ${DELETE USER BUTTON}
    Click Button    ${DELETE USER BUTTON}
    ${PERMISSIONS WERE REMOVED FROM EMAIL}    Replace String    ${PERMISSIONS WERE REMOVED FROM}    {{email}}    ${user email}
    Check For Alert    ${PERMISSIONS WERE REMOVED FROM EMAIL}
    Wait Until Element Is Not Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]

Check For Alert
    [arguments]    ${alert text}
    Wait Until Element Is Visible    ${ALERT}
    Element Should Be Visible    ${ALERT}
    Element Text Should Be    ${ALERT}    ${alert text}
    Wait Until Page Does Not Contain Element    ${ALERT}

Check For Alert Dismissable
    [arguments]    ${alert text}
    Wait Until Elements Are Visible    ${ALERT}    ${ALERT CLOSE}
    Element Text Should Be    ${ALERT}    ${alert text}
    Click Element    ${ALERT CLOSE}
    Wait Until Page Does Not Contain Element    ${ALERT}


Verify In System
    [arguments]    ${system name}
    Wait Until Element Is Visible    //h1[@ng-if='gettingSystem.success' and contains(text(), '${system name}')]

Failure Tasks
    Capture Page Screenshot    selenium-screenshot-{index}.png
    Close Browser

Wait Until Elements Are Visible
    [arguments]    @{elements}
    :FOR     ${element}  IN  @{elements}
    \  Wait Until Element Is Visible    ${element}

Form Validation
    [arguments]    ${form name}    ${first name}=mark    ${last name}=hamill    ${email}=${EMAIL OWNER}    ${password}=${BASE PASSWORD}
    Run Keyword If    "${form name}"=="Log In"    Log In Form Validation   ${email}    ${password}
    Run Keyword If    "${form name}"=="Register"    Register Form Validation    ${first name}    ${last name}    ${email}    ${password}

Log In Form Validation
    [Arguments]    ${email}    ${password}
    Wait Until Elements Are Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Elements Are Visible    ${EMAIL INPUT}    ${PASSWORD INPUT}    ${LOG IN BUTTON}
    Input Text    ${EMAIL INPUT}    ${email}
    Input Text    ${PASSWORD INPUT}    ${password}
    click button    ${LOG IN BUTTON}

Register Form Validation
    [arguments]    ${first name}    ${last name}    ${email}    ${password}
    Wait Until Elements Are Visible    ${REGISTER FIRST NAME INPUT}    ${REGISTER LAST NAME INPUT}    ${REGISTER EMAIL INPUT}    ${REGISTER PASSWORD INPUT}    ${CREATE ACCOUNT BUTTON}
    Input Text    ${REGISTER FIRST NAME INPUT}    ${first name}
    Input Text    ${REGISTER LAST NAME INPUT}    ${last name}
    Input Text    ${REGISTER EMAIL INPUT}    ${email}
    Input Text    ${REGISTER PASSWORD INPUT}    ${password}
    click button    ${CREATE ACCOUNT BUTTON}

