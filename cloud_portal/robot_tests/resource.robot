*** Settings ***

Library           Selenium2Library    screenshot_root_directory=\Screenshots    run_on_failure=Failure Tasks
Library           ImapLibrary
Library           String
Library           NoptixLibrary/
Resource          variables.robot

*** Keywords ***
Open Browser and go to URL
    [Arguments]    ${url}
    Open Browser    ${url}    Chrome
#    Maximize Browser Window
    Set Selenium Speed    0

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

Log Out
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Click Element    ${ACCOUNT DROPDOWN}
    Wait Until Element Is Visible    ${LOG OUT BUTTON}
    Click Link    ${LOG OUT BUTTON}
    Validate Log Out

Validate Log Out
    Wait Until Element Is Visible    ${ANONYMOUS BODY}

Register
    [arguments]    ${first name}    ${last name}    ${email}    ${password}
    Wait Until Elements Are Visible    ${REGISTER FIRST NAME INPUT}    ${REGISTER LAST NAME INPUT}    ${REGISTER EMAIL INPUT}    ${REGISTER PASSWORD INPUT}    ${CREATE ACCOUNT BUTTON}
    Input Text    ${REGISTER FIRST NAME INPUT}    ${first name}
    Input Text    ${REGISTER LAST NAME INPUT}    ${last name}
    Input Text    ${REGISTER EMAIL INPUT}    ${email}
    Input Text    ${REGISTER PASSWORD INPUT}    ${password}
    Click Button    ${CREATE ACCOUNT BUTTON}

Validate Register Success
    [arguments]    ${location}=${url}/register/success
    Wait Until Element Is Visible    ${ACCOUNT CREATION SUCCESS}
    Location Should Be    ${location}

Validate Register Email Received
    [arguments]    ${recipient}
    Open Mailbox    host=imap.gmail.com    password=qweasd!@#    port=993    user=noptixqa@gmail.com    is_secure=True
    ${email}    Wait For Email    recipient=${recipient}    timeout=120
    Should Not Be Equal    ${email}    ${EMPTY}
    Close Mailbox

Get Activation Link
    [arguments]    ${recipient}
    Open Mailbox    host=imap.gmail.com    password=qweasd!@#    port=993    user=noptixqa@gmail.com    is_secure=True
    ${email}    Wait For Email    recipient=${recipient}    timeout=120
    ${links}    Get Links From Email    ${email}
    Close Mailbox
    Return From Keyword    @{links}[1]

Activate
    [arguments]    ${email}
    ${link}    Get Activation Link    ${email}
    Go To    ${link}
    Wait Until Element Is Visible    ${ACTIVATION SUCCESS}
    Element Should Be Visible    ${ACTIVATION SUCCESS}
    Location Should Be    ${url}/activate/success

Edit User Permissions In Systems
    [arguments]    ${user email}    ${permissions}
    Wait Until Element Is Not Visible    ${SHARE MODAL}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'Edit')]/..
    Click Element    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),'Edit')]/..
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
    Element Should Be Visible    ${ALERT}
    Element Text Should Be    ${ALERT}    ${alert text}
    Click Element    ${ALERT CLOSE}

Verify In System
    [arguments]    ${system name}
    Wait Until Element Is Visible    //h1[@ng-if='gettingSystem.success' and contains(text(), '${system name}')]
    Element Should Be Visible    //h1[@ng-if='gettingSystem.success' and contains(text(), '${system name}')]

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

Get Reset Password Link
    [arguments]    ${recipient}
    Open Mailbox    host=imap.gmail.com    password=qweasd!@#    port=993    user=noptixqa@gmail.com    is_secure=True
    ${email}    Wait For Email    recipient=${recipient}    timeout=120    subject=${RESET PASSWORD EMAIL SUBJECT}
    ${links}    Get Links From Email    ${email}
    Close Mailbox
    Return From Keyword    @{links}[1]