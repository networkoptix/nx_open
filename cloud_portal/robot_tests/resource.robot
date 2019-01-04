*** Settings ***
Library           SeleniumLibrary    run_on_failure=Failure Tasks
Library           NoptixImapLibrary/
Library           String
Library           NoptixLibrary/
Resource          variables.robot
Resource          ${variables_file}

*** variables ***
${headless}    false
${directory}    ${SCREENSHOTDIRECTORY}
${variables_file}    variables-env.robot
${docker}    false
@{chrome_arguments}    --disable-infobars    --headless    --disable-gpu    --no-sandbox

*** Keywords ***
Open Browser and go to URL
    [Arguments]    ${url}
    run keyword if    "${docker}"=="false"    Regular Open Browser    ${url}
    ...          ELSE    Docker Open Browser    ${url}
    Set Selenium Speed    0
    Set Selenium Timeout    10
    Check Language
    Go To    ${url}

Regular Open Browser
    [Arguments]    ${url}
    Set Screenshot Directory    ${SCREENSHOT_DIRECTORY}
    Open Browser    ${ENV}    ${BROWSER}
    Set Window Size    1920    1080

Docker Open Browser
    [Arguments]    ${url}
    Set Screenshot Directory    ${SCREENSHOT_DIRECTORY}
    ${chrome_options}=    Set Chrome Options
    Create Webdriver    Chrome    chrome_options=${chrome_options}
    Sleep    20
    Go to    ${url}

Set Chrome Options
    [Documentation]    Set Chrome options for headless mode
    ${options}=    Evaluate    sys.modules['selenium.webdriver'].ChromeOptions()    sys, selenium.webdriver
    : FOR    ${option}    IN    @{chrome_arguments}
    \    Call Method    ${options}    add_argument    ${option}
    [Return]    ${options}

Check Language
#    Wait Until Page Contains Element    ${LANGUAGE DROPDOWN}/span[@lang='en_US']
    Register Keyword To Run On Failure    NONE
    ${status}    ${value}=    Run Keyword And Ignore Error    Wait Until Element Is Visible    ${LANGUAGE DROPDOWN}/span[@lang='${LANGUAGE}']    2
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    "${status}"=="FAIL"    Set Language

Set Language
    [arguments]    ${lang}=${LANGUAGE}
    Wait Until Element Is Visible    ${LANGUAGE DROPDOWN}    20
    Click Button    ${LANGUAGE DROPDOWN}
    Wait Until Element Is Visible    ${LANGUAGE TO SELECT}
    Click Element    ${LANGUAGE TO SELECT}
    Wait Until Element Is Visible    ${LANGUAGE DROPDOWN}/span[@lang='${lang}']    5
    Sleep    1    #to wait for language to fully change before continuing.  This caused issues with login.

Log In
    [arguments]    ${email}    ${password}    ${button}=${LOG IN NAV BAR}
    Run Keyword Unless    '''${button}''' == "None"    Wait Until Element Is Visible    ${button}
    Run Keyword Unless    '''${button}''' == "None"    Click Link    ${button}
    Wait Until Elements Are Visible    ${EMAIL INPUT}    ${PASSWORD INPUT}    ${REMEMBER ME CHECKBOX VISIBLE}    ${FORGOT PASSWORD}    ${LOG IN CLOSE BUTTON}
    Input Text    ${EMAIL INPUT}    ${email}
    Input Text    ${PASSWORD INPUT}    ${password}
    Wait Until Element Is Visible    ${LOG IN BUTTON}
    Click Button    ${LOG IN BUTTON}

Validate Log In
    Wait Until Page Contains Element    ${AUTHORIZED BODY}
    Wait Until Elements Are Visible    ${ACCOUNT DROPDOWN}
    Check Language
    Sleep    1    #this is a test to see if it eliminates a problem with the login dialog popping up on logout

Log Out
    Wait Until Page Does Not Contain Element    ${BACKDROP}
    Wait Until Page Contains Element    ${LOG OUT BUTTON}
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Sleep    .05    #Ubuntu was clicking too soon
    Click Button    ${ACCOUNT DROPDOWN}
    Wait Until Element Is Visible    ${LOG OUT BUTTON}
    Click Link    ${LOG OUT BUTTON}
    Validate Log Out

Validate Log Out
    Wait Until Element Is Not Visible    ${BACKDROP}
    Wait Until Page Contains Element    ${ANONYMOUS BODY}

Register
    [arguments]    ${first name}    ${last name}    ${email}    ${password}    ${checked}=false
    Wait Until Elements Are Visible    ${REGISTER FIRST NAME INPUT}    ${REGISTER LAST NAME INPUT}    ${REGISTER PASSWORD INPUT}    ${CREATE ACCOUNT BUTTON}
    Input Text    ${REGISTER FIRST NAME INPUT}    ${first name}
    Input Text    ${REGISTER LAST NAME INPUT}    ${last name}
    ${read only}    Run Keyword And Return Status    Wait Until Element Is Visible    ${REGISTER EMAIL INPUT LOCKED}
    Run Keyword Unless    ${read only}    Input Text    ${REGISTER EMAIL INPUT}    ${email}
    Input Text    ${REGISTER PASSWORD INPUT}    ${password}
    Run Keyword If    "${checked}"=="false"    Click Element    ${TERMS AND CONDITIONS CHECKBOX VISIBLE}
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
    Delete Email    ${email}
    Close Mailbox

Get Email Link
    [arguments]    ${recipient}    ${link type}
    Open Mailbox    host=${BASE HOST}    password=${BASE EMAIL PASSWORD}    port=${BASE PORT}    user=${BASE EMAIL}    is_secure=True
    ${email}    Wait For Email    recipient=${recipient}    timeout=120    status=UNSEEN
    Run Keyword If    "${link type}"=="activate"    Check Email Subject    ${email}    ${ACTIVATE YOUR ACCOUNT EMAIL SUBJECT}    ${BASE EMAIL}    ${BASE EMAIL PASSWORD}    ${BASE HOST}    ${BASE PORT}
    Run Keyword If    "${link type}"=="restore_password"    Check Email Subject    ${email}    ${RESET PASSWORD EMAIL SUBJECT}    ${BASE EMAIL}    ${BASE EMAIL PASSWORD}    ${BASE HOST}    ${BASE PORT}
    ${INVITED TO SYSTEM EMAIL SUBJECT UNREGISTERED}    Replace String    ${INVITED TO SYSTEM EMAIL SUBJECT UNREGISTERED}    {{message.sharer_name}}    ${TEST FIRST NAME} ${TEST LAST NAME}
    ${INVITED TO SYSTEM EMAIL SUBJECT UNREGISTERED}    Replace String    ${INVITED TO SYSTEM EMAIL SUBJECT UNREGISTERED}    %PRODUCT_NAME%    Nx Cloud
    Run Keyword If    "${link type}"=="register"    Check Email Subject    ${email}    ${INVITED TO SYSTEM EMAIL SUBJECT UNREGISTERED}    ${BASE EMAIL}    ${BASE EMAIL PASSWORD}    ${BASE HOST}    ${BASE PORT}
    ${links}    Get NX Links From Email    ${email}    ${link type}
    log    ${links}
    Delete Email    ${email}
    Close Mailbox
    Return From Keyword    ${links}

Activate
    [arguments]    ${email}
    ${link}    Get Email Link    ${email}    activate
    Go To    ${link}
    Wait Until Element Is Visible    ${ACTIVATION SUCCESS}
    Element Should Be Visible    ${ACTIVATION SUCCESS}
    Location Should Be    ${url}/activate/success

Restore password
    [arguments]    ${email}
    #log in to user to make sure their language is set to the current
    Open Browser and go to URL    ${url}
    Log In    ${email}    ${password}
    Validate Log In
    Log Out
    Validate Log Out
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    Wait Until Element Is Visible    ${RESET EMAIL SENT MESSAGE}
    ${link}    Get Email Link    ${email}    restore_password
    Go To    ${link}
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}
    Sleep    .5
    Input Text    ${RESET PASSWORD INPUT}    ${BASE PASSWORD}
    Click Button    ${SAVE PASSWORD}
    Wait Until Elements Are Visible    ${RESET SUCCESS MESSAGE}    ${RESET SUCCESS LOG IN LINK}
    Click Link    ${RESET SUCCESS LOG IN LINK}
    Log In    ${email}    ${BASE PASSWORD}    None
    Validate Log In
    Close Browser

Share To
    [arguments]    ${random email}    ${permissions}
    ${log}    get_browser_log
    log    ${log}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Button    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE MODAL}//nx-permissions-select//li//span[text()='${permissions}']
    Click Link    ${SHARE MODAL}//nx-permissions-select//li//span[text()='${permissions}']/..
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}

Edit User Permissions In Systems
    [arguments]    ${user email}    ${permissions}
    Wait Until Element Is Not Visible    ${SHARE MODAL}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),"${EDIT USER BUTTON TEXT}")]/..
    Click Element    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/a[@ng-click='editShare(user)']/span[contains(text(),"${EDIT USER BUTTON TEXT}")]/..
    Wait Until Element Is Visible    ${EDIT PERMISSIONS DROPDOWN}
    Click Element    ${EDIT PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE MODAL}//nx-permissions-select//li//span[text()='${permissions}']
    Click Link    ${SHARE MODAL}//nx-permissions-select//li//span[text()='${permissions}']/..
    Click Button    ${EDIT PERMISSIONS SAVE}
    Wait Until Page Does Not Contain Element    ${SHARE MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}

Check User Permissions
    [arguments]    ${user email}    ${permissions}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/span["${permissions}"]
    Element Should Be Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/span["${permissions}"]

Remove User Permissions
    [arguments]    ${user email}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span[contains(text(),'${DELETE USER BUTTON TEXT}')]
    Click Element    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span['&nbsp&nbspDelete']
    Wait Until Element Is Visible    ${DELETE USER BUTTON}
    Click Button    ${DELETE USER BUTTON}
    ${PERMISSIONS WERE REMOVED FROM EMAIL}    Replace String    ${PERMISSIONS WERE REMOVED FROM}    %email%    ${user email}
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
    Capture Page Screenshot    selenium-screenshot-${LANGUAGE}{index}.png

Wait Until Elements Are Visible
    [arguments]    @{elements}
    :FOR     ${element}  IN  @{elements}
    \  Wait Until Element Is Visible    ${element}

Elements Should Not Be Visible
    [arguments]    @{elements}
    :FOR     ${element}  IN  @{elements}
    \  Element Should Not Be Visible    ${element}

#Reset resources
Clean up email noperm
    Register Keyword To Run On Failure    None
    Open Browser and Go To URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO_TESTS SYSTEM ID}
    Register Keyword To Run On Failure    NONE
    Run Keyword And Ignore Error    Remove User Permissions    ${EMAIL NOPERM}
    Register Keyword To Run On Failure    Failure Tasks
    Close Browser

Clean up random emails
    Register Keyword To Run On Failure    None
    Open Browser and Go To URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO_TESTS SYSTEM ID}
    ${status}    Run Keyword And Return Status    Wait Until Element Is Visible    //div[@process-loading='gettingSystemUsers']//tbody//tr//td[contains(text(), 'noptixautoqa+15')]
    Run Keyword If    ${status}    Find and remove emails
    Close Browser

Find and remove emails
    ${random emails}    Get WebElements    //div[@process-loading='gettingSystemUsers']//tbody//tr//td[contains(text(), 'noptixautoqa+15')]
    :FOR    ${element}    IN    @{random emails}
    \  ${email}    Get Text    ${element}
    \  Mouse Over    ${element}
    \  Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span['&nbsp&nbspDelete']
    \  Click Element    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${email}')]/following-sibling::td/a[@ng-click='unshare(user)']/span['&nbsp&nbspDelete']
    \  Wait Until Element Is Visible    ${DELETE USER BUTTON}
    \  Click Button    ${DELETE USER BUTTON}
    \  ${PERMISSIONS WERE REMOVED FROM EMAIL}    Replace String    ${PERMISSIONS WERE REMOVED FROM}    %email%    ${email}
    \  Check For Alert    ${PERMISSIONS WERE REMOVED FROM EMAIL}
    \  Wait Until Element Is Not Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${email}')]

Reset user noperm first/last name
    Register Keyword To Run On Failure    None
    Open Browser and go to URL    ${url}
    Go To    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In

    Run Keyword And Ignore Error    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    nameChanged
    Run Keyword And Ignore Error    Wait Until Textfield Contains    ${ACCOUNT LAST NAME}    nameChanged
    Register Keyword To Run On Failure    Failure Tasks

    Clear Element Text    ${ACCOUNT FIRST NAME}
    Input Text    ${ACCOUNT FIRST NAME}    ${TEST FIRST NAME}
    Clear Element Text    ${ACCOUNT LAST NAME}
    Input Text    ${ACCOUNT LAST NAME}    ${TEST LAST NAME}
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    # In case Kyle forgets about this it's a test to see if it fixes a problem with not changing the name back in some cases
    Sleep    2
    Close Browser

Reset user owner first/last name
    Register Keyword To Run On Failure    None
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL OWNER}    ${password}    button=None
    Validate Log In

    Run Keyword And Ignore Error    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    newFirstName
    Run Keyword And Ignore Error    Wait Until Textfield Contains    ${ACCOUNT LAST NAME}    newLastName
    Register Keyword To Run On Failure    Failure Tasks
    Sleep    1
    Clear Element Text    ${ACCOUNT FIRST NAME}
    Input Text    ${ACCOUNT FIRST NAME}    ${TEST FIRST NAME}
    Clear Element Text    ${ACCOUNT LAST NAME}
    Input Text    ${ACCOUNT LAST NAME}    ${TEST LAST NAME}
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Close Browser

Add notowner
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${EMAIL NOT OWNER}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Check User Permissions    ${EMAIL NOT OWNER}    ${CUSTOM TEXT}
    Close Browser

Make sure notowner is in the system
    Register Keyword To Run On Failure    None
    Open Browser and Go To URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO_TESTS SYSTEM ID}
    ${status}    Run Keyword And Return Status    Wait Until Element Is Visible    ${NOT OWNER IN SYSTEM}
    Run Keyword Unless    ${status}    Add notowner
    Close Browser

Reset System Names
    Open Browser and go to URL    ${url}/systems/${AUTOTESTS OFFLINE SYSTEM ID}
    Log In    ${EMAIL OWNER}    ${BASE PASSWORD}    None
    Validate Log In
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}    ${RENAME INPUT}
    Clear Element Text    ${RENAME INPUT}
    Input Text    ${RENAME INPUT}    Auto Tests 2
    Click Button    ${RENAME SAVE}
    Check For Alert    ${SYSTEM NAME SAVED}
    Verify In System    Auto Tests 2

    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Click Button    ${RENAME SYSTEM}
    Wait Until Elements Are Visible    ${RENAME CANCEL}    ${RENAME SAVE}    ${RENAME INPUT}
    Clear Element Text    ${RENAME INPUT}
    Input Text    ${RENAME INPUT}    Auto Tests
    Click Button    ${RENAME SAVE}
    Check For Alert    ${SYSTEM NAME SAVED}
    Verify In System    Auto Tests
    Close Browser