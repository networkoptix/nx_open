*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close Browser
*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${ENV}
${FIRST NAME IS REQUIRED}      //span[@ng-if='accountForm.firstName.$touched && accountForm.firstName.$error.required' and contains(text(),'${FIRST NAME IS REQUIRED TEXT}')]
${LAST NAME IS REQUIRED}       //span[@ng-if='accountForm.lastName.$touched && accountForm.lastName.$error.required' and contains(text(),'${LAST NAME IS REQUIRED TEXT}')]

*** Keywords ***
Verify In Account Page
    Location Should Be    ${url}/account
    Wait Until Elements Are Visible    ${ACCOUNT EMAIL}    ${ACCOUNT FIRST NAME}    ${ACCOUNT LAST NAME}    ${ACCOUNT SAVE}    ${ACCOUNT LANGUAGE DROPDOWN}    ${ACCOUNT SUBSCRIBE CHECKBOX}    ${ACCOUNT DROPDOWN}
    sleep    .5

Restart
    ${status}    Run Keyword And Return Status    Validate Log In
    Run Keyword If    ${status}    Log Out
    Validate Log Out
    Go To    ${url}

Reset DB and Open New Browser On Failure
    Close Browser
    Reset user noperm first/last name
    Open Browser and go to URL    ${url}

*** Test Cases ***
Can access the account page from dropdown
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Click Link    ${ACCOUNT DROPDOWN}
    Wait Until Element Is Visible    ${ACCOUNT SETTINGS BUTTON}
    Click Link    ${ACCOUNT SETTINGS BUTTON}
    Verify in account page

Can access the account page from direct link while logged in
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Go To    ${url}/account
    Verify in account page

Accessing the account page from a direct link while logged out asks for login, closing log in takes you to main page
    Go To    ${url}/account
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Click Button    ${LOG IN CLOSE BUTTON}
    Location Should Be    ${url}/

Accessing the account page from a direct link while logged out asks for login, on valid login takes you to account page
    Go To    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify in account page

Subscribe check box is automatically checked after registering
    [tags]    email
    ${random email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${random email}    ${password}
    Activate    ${random email}
    Log In    ${random email}    ${password}
    Validate Log In
    Go To    ${url}/account
    Verify In Account Page
    ${checked}    Get Element Attribute    ${ACCOUNT SUBSCRIBE CHECKBOX}    checked
    Should Be True    "${checked}"

Unchecking check box and saving maintains that setting
    [tags]    email
    ${random email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${random email}    ${password}
    Activate    ${random email}
    Log In    ${random email}    ${password}
    Validate Log In
    Go To    ${url}/account
    Verify In Account Page

    Click Element    ${ACCOUNT SUBSCRIBE CHECKBOX}
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Close Browser
    Open Browser and go to URL    ${url}/account
    Log In    ${random email}    ${password}    button=None
    Validate Log In
    ${checked}    Get Element Attribute    ${ACCOUNT SUBSCRIBE CHECKBOX}    checked
    Should Not Be True    "${checked}"=="true"

Checking check box and saving maintains that setting
    [tags]    email
    ${random email}    Get Random Email    ${BASE EMAIL}
    Go To    ${url}/register
    Register    mark    hamill    ${random email}    ${password}    false
    Activate    ${random email}
    Log In    ${random email}    ${password}
    Validate Log In
    Go To    ${url}/account
    Verify In Account Page
    Click Element    ${ACCOUNT SUBSCRIBE CHECKBOX}
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Close Browser
    Open Browser and go to URL    ${url}/account
    Log In    ${random email}    ${password}    button=None
    Validate Log In
    ${checked}    Get Element Attribute    ${ACCOUNT SUBSCRIBE CHECKBOX}    checked
    Should Be True    "${checked}"

Changing first name and saving maintains that setting
    Go To    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    Clear Element Text    ${ACCOUNT FIRST NAME}
    Input Text    ${ACCOUNT FIRST NAME}    nameChanged
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Close Browser
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    nameChanged
    Clear Element Text    ${ACCOUNT FIRST NAME}
    Input Text    ${ACCOUNT FIRST NAME}    ${TEST FIRST NAME}
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}


Changing last name and saving maintains that setting
    Go To    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    Input Text    ${ACCOUNT LAST NAME}    nameChanged
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Close Browser
    Open Browser and go to URL    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    Wait Until Textfield Contains    ${ACCOUNT LAST NAME}    nameChanged
    Input Text    ${ACCOUNT LAST NAME}    ${TEST LAST NAME}
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}

First name is required
    Go To    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    Input Text    ${ACCOUNT FIRST NAME}    ${EMPTY}
    Click Button    ${ACCOUNT SAVE}
    Wait Until Element Is Visible    ${ACCOUNT FIRST NAME}/parent::div/parent::div[contains(@class, "has-error")]
    Element Should Be Visible    ${FIRST NAME IS REQUIRED}

Last name is required
    Go To    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    Input Text    ${ACCOUNT LAST NAME}    ${EMPTY}
    Click Button    ${ACCOUNT SAVE}
    Wait Until Element Is Visible    ${ACCOUNT LAST NAME}/parent::div/parent::div[contains(@class, "has-error")]
    Element Should Be Visible    ${LAST NAME IS REQUIRED}

SPACE for first name is not valid
    Go To    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    Input Text    ${ACCOUNT FIRST NAME}    ${SPACE}
    Click Button    ${ACCOUNT SAVE}
    Wait Until Element Is Visible    ${ACCOUNT FIRST NAME}/parent::div/parent::div[contains(@class, "has-error")]
    Element Should Be Visible    ${FIRST NAME IS REQUIRED}

SPACE for last name is not valid
    Go To    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    Input Text    ${ACCOUNT LAST NAME}    ${SPACE}
    Click Button    ${ACCOUNT SAVE}
    Wait Until Element Is Visible    ${ACCOUNT LAST NAME}/parent::div/parent::div[contains(@class, "has-error")]
    Element Should Be Visible    ${LAST NAME IS REQUIRED}

Email field is un-editable
    Go To    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    Verify In Account Page
    ${read only}    Get Element Attribute    ${ACCOUNT EMAIL}    readOnly
    Should Be True    "${read only}"

Langauge is changeable on the account page
    Go To    ${url}/account
    Log In    ${EMAIL NOPERM}    ${password}    button=None
    Validate Log In
    :FOR    ${lang}    ${account}   IN ZIP    ${LANGUAGES LIST}    ${LANGUAGES ACCOUNT TEXT LIST}
    \  Sleep    1
    \  Verify In Account Page
    \  Run Keyword Unless    "${lang}"=="${LANGUAGE}"    Click Button    ${ACCOUNT LANGUAGE DROPDOWN}
    \  Run Keyword Unless    "${lang}"=="${LANGUAGE}"    Wait Until Element Is Visible    //form[@name='accountForm']//a[@ng-click='changeLanguage(lang.language)']/span[@lang='${lang}']
    \  Run Keyword Unless    "${lang}"=="${LANGUAGE}"    Click Element    //form[@name='accountForm']//a[@ng-click='changeLanguage(lang.language)']/span[@lang='${lang}']
    \  Run Keyword Unless    "${lang}"=="${LANGUAGE}"    Click Button    ${ACCOUNT SAVE}
    \  Sleep    1    #to allow the system to change languages
    \  Run Keyword Unless    "${lang}"=="${LANGUAGE}"    Wait Until Element Is Visible    //h1['${account}']
    Wait Until Element Is Visible    ${ACCOUNT LANGUAGE DROPDOWN}
    Click Button    ${ACCOUNT LANGUAGE DROPDOWN}
    Wait Until Element Is Visible    //form[@name='accountForm']//a[@ng-click='changeLanguage(lang.language)']/span[@lang='${LANGUAGE}']
    Click Element    //form[@name='accountForm']//a[@ng-click='changeLanguage(lang.language)']/span[@lang='${LANGUAGE}']
    Click Button    ${ACCOUNT SAVE}
    Sleep    1
    Verify In Account Page
    Wait Until Element Is Visible    //h1['${ACCOUNT TEXT}']