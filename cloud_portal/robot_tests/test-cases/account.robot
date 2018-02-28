*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${ENV}
${FIRST NAME IS REQUIRED}      //span[@ng-if='accountForm.firstName.$touched && accountForm.firstName.$error.required' and contains(text(),'${FIRST NAME IS REQUIRED TEXT}')]
${LAST NAME IS REQUIRED}       //span[@ng-if='accountForm.lastName.$touched && accountForm.lastName.$error.required' and contains(text(),'${LAST NAME IS REQUIRED TEXT}')]

*** Keywords ***
Verify In Account Page
    Location Should Be    ${url}/account
    Wait Until Elements Are Visible    ${ACCOUNT EMAIL}    ${ACCOUNT FIRST NAME}    ${ACCOUNT LAST NAME}    ${ACCOUNT SAVE}    ${ACCOUNT SUBSCRIBE CHECKBOX}

*** Test Cases ***
Can access the account page from dropdown
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Click Link    ${ACCOUNT DROPDOWN}
    Wait Until Element Is Visible    ${ACCOUNT SETTINGS BUTTON}
    Click Link    ${ACCOUNT SETTINGS BUTTON}
    Verify in account page
    Close Browser

Can access the account page from direct link while logged in
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Go To    ${url}/account
    Verify in account page
    Close Browser

Accessing the account page from a direct link while logged out asks for login, closing log in takes you to main page
    Open Browser and go to URL    ${url}/account
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Click Button    ${LOG IN CLOSE BUTTON}
    Location Should Be    ${url}/
    Close Browser

Accessing the account page from a direct link while logged out asks for login, on valid login takes you to account page
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify in account page
    Close Browser

Check box is checked when registering with it checked
    [tags]    email
    ${random email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${random email}    ${password}
    Activate    ${random email}
    Log In    ${random email}    ${password}
    Validate Log In
    Go To    ${url}/account
    Verify In Account Page
    ${checked}    Get Element Attribute    ${ACCOUNT SUBSCRIBE CHECKBOX}    checked
    Should Be True    ${checked}
    Close Browser

Check box is not checked when registering with it not checked
    [tags]    email
    ${random email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${random email}    ${password}    false
    Activate    ${random email}
    Log In    ${random email}    ${password}
    Validate Log In
    Go To    ${url}/account
    Verify In Account Page
    ${checked}    Get Element Attribute    ${ACCOUNT SUBSCRIBE CHECKBOX}    checked
    Should Not Be True    ${checked}
    Close Browser

Unchecking check box and saving maintains that setting
    [tags]    email
    ${random email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${random email}    ${password}
    Activate    ${random email}
    Log In    ${random email}    ${password}
    Validate Log In
    Go To    ${url}/account
    Verify In Account Page
    Click Element    ${ACCOUNT SUBSCRIBE CHECKBOX}
    Click Button    ${ACCOUNT SAVE}
    Close Browser
    Open Browser and go to URL    ${url}/account
    Log In    ${random email}    ${password}    button=None
    ${checked}    Get Element Attribute    ${ACCOUNT SUBSCRIBE CHECKBOX}    checked
    Should Not Be True    ${checked}
    Close Browser


Checking check box and saving maintains that setting
    [tags]    email
    ${random email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${random email}    ${password}    false
    Activate    ${random email}
    Log In    ${random email}    ${password}
    Validate Log In
    Go To    ${url}/account
    Verify In Account Page
    Click Element    ${ACCOUNT SUBSCRIBE CHECKBOX}
    Click Button    ${ACCOUNT SAVE}
    Close Browser
    Open Browser and go to URL    ${url}/account
    Log In    ${random email}    ${password}    button=None
    ${checked}    Get Element Attribute    ${ACCOUNT SUBSCRIBE CHECKBOX}    checked
    Should Be True    ${checked}
    Close Browser

Changing first name and saving maintains that setting
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    Input Text    ${ACCOUNT FIRST NAME}    nameChanged
    Click Button    ${ACCOUNT SAVE}
    Close Browser
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Verify In Account Page
    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    nameChanged
    Input Text    ${ACCOUNT FIRST NAME}    ${TEST FIRST NAME}
    Click Button    ${ACCOUNT SAVE}
    Close Browser


Changing last name and saving maintains that setting
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    Input Text    ${ACCOUNT LAST NAME}    nameChanged
    Click Button    ${ACCOUNT SAVE}
    Close Browser
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Verify In Account Page
    Wait Until Textfield Contains    ${ACCOUNT LAST NAME}    nameChanged
    Input Text    ${ACCOUNT LAST NAME}    ${TEST FIRST NAME}
    Click Button    ${ACCOUNT SAVE}
    Close Browser

First name is required
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    Input Text    ${ACCOUNT FIRST NAME}    ${EMPTY}
    Click Button    ${ACCOUNT SAVE}
    Wait Until Element Is Visible    ${ACCOUNT FIRST NAME}/parent::div/parent::div[contains(@class, "has-error")]
    Element Should Be Visible    ${FIRST NAME IS REQUIRED}
    Close Browser

Last name is required
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    Input Text    ${ACCOUNT LAST NAME}    ${EMPTY}
    Click Button    ${ACCOUNT SAVE}
    Wait Until Element Is Visible    ${ACCOUNT LAST NAME}/parent::div/parent::div[contains(@class, "has-error")]
    Element Should Be Visible    ${LAST NAME IS REQUIRED}
    Close Browser

Language changes when saved and maintains that setting

Email field is un-editable
