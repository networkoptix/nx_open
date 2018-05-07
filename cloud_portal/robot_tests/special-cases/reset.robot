*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers
Test Teardown     Close Browser

*** Variables ***
${password}    ${BASE PASSWORD}
${url}    ${ENV}

*** Keywords ***
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

Add notowner
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${EMAIL NOT OWNER}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Check User Permissions    ${EMAIL NOT OWNER}    ${CUSTOM TEXT}

*** Test Cases ***
Clean up email noperm
    Register Keyword To Run On Failure    None
    Open Browser and Go To URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO_TESTS SYSTEM ID}
    Run Keyword And Ignore Error    Remove User Permissions    ${EMAIL NOPERM}

Clean up random emails
    Register Keyword To Run On Failure    None
    Open Browser and Go To URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO_TESTS SYSTEM ID}
    ${status}    Run Keyword And Return Status    Wait Until Element Is Visible    //div[@process-loading='gettingSystemUsers']//tbody//tr//td[contains(text(), 'noptixautoqa+15')]
    Run Keyword If    ${status}    Find and remove emails

Make sure notowner is in the system
    Register Keyword To Run On Failure    None/
    Open Browser and Go To URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO_TESTS SYSTEM ID}
    ${status}    Run Keyword And Return Status    Wait Until Element Is Visible    //div[@process-loading='gettingSystemUsers']//tbody//tr//td[contains(text(), 'noptixautoqa+notowner@gmail.com')]
    Run Keyword Unless    ${status}    Add notowner

Clean up noperm first/last name
    Register Keyword To Run On Failure    None
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Run Keyword And Ignore Error    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    nameChanged
    Run Keyword And Ignore Error    Wait Until Textfield Contains    ${ACCOUNT LAST NAME}    nameChanged

    Clear Element Text    ${ACCOUNT FIRST NAME}
    Input Text    ${ACCOUNT FIRST NAME}    ${TEST FIRST NAME}
    Clear Element Text    ${ACCOUNT LAST NAME}
    Input Text    ${ACCOUNT LAST NAME}    ${TEST LAST NAME}
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}

Clean up owner first/last name
    Register Keyword To Run On Failure    None
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL OWNER}    ${password}    button=None
    Validate Log In
    Run Keyword And Ignore Error    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    newFirstName
    Run Keyword And Ignore Error    Wait Until Textfield Contains    ${ACCOUNT LAST NAME}    newLastName

    Clear Element Text    ${ACCOUNT FIRST NAME}
    Input Text    ${ACCOUNT FIRST NAME}    ${TEST FIRST NAME}
    Clear Element Text    ${ACCOUNT LAST NAME}
    Input Text    ${ACCOUNT LAST NAME}    ${TEST LAST NAME}
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}