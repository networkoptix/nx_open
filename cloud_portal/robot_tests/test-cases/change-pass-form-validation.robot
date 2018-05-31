*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Setup       Open Change Password Dialog
Suite Teardown    Close Browser
Test Teardown     Run Keyword If Test Failed    Restart
Test Template     Test Passwords Invalid
Force Tags        form

*** Variables ***
${url}    ${ENV}
${no upper password}           adrhartjad
${7char password}              asdfghj
${common password}             yyyyyyyy
${weak password}               asqwerdf

${PASSWORD IS REQUIRED}            //span[@ng-if='passwordInput.password.$error.required' and contains(text(),'${PASSWORD IS REQUIRED TEXT}')]
${PASSWORD SPECIAL CHARS}          //span[@ng-if='passwordInput.password.$error.pattern' and contains(text(),'${PASSWORD SPECIAL CHARS TEXT}')]
${PASSWORD TOO SHORT}              //span[contains(@ng-if,'passwordInput.password.$error.minlength &&') and contains(@ng-if,'!passwordInput.password.$error.pattern') and contains(text(),'${PASSWORD TOO SHORT TEXT}')]
${PASSWORD TOO COMMON}             //span[contains(@ng-if,'passwordInput.password.$error.common &&') and contains(@ng-if,'!passwordInput.password.$error.pattern &&') and contains(@ng-if,'!passwordInput.password.$error.required') and contains(text(),'${PASSWORD TOO COMMON TEXT}')]
${PASSWORD IS WEAK}                //span[contains(@ng-if,'passwordInput.password.$error.common &&') and contains(@ng-if,'!passwordInput.password.$error.pattern &&') and contains(@ng-if,'!passwordInput.password.$error.required') and contains(text(),'${PASSWORD IS WEAK TEXT}')]
${CURRENT PASSWORD IS REQUIRED}    //span[contains(@ng-if,'passwordForm.password.$touched &&') and contains(@ng-if,'passwordForm.password.$error.required') and contains(text(),'${CURRENT PASSWORD IS REQUIRED TEXT}')]

*** Test Cases ***            OLD PW                    NEW PW
Invalid Old Password          ${7char password}         ${BASE PASSWORD}
Empty Old password            ${EMPTY}                  ${BASE PASSWORD}
Invalid New Password 1        ${BASE PASSWORD}          ${7char password}
Invalid New Password 2        ${BASE PASSWORD}          ${no upper password}
Invalid New Password 3        ${BASE PASSWORD}          ${common password}
Invalid New Password 4        ${BASE PASSWORD}          ${weak password}
Invalid New Password 5        ${BASE PASSWORD}          ${CYRILLIC TEXT}
Invalid New Password 6        ${BASE PASSWORD}          ${SMILEY TEXT}
Invalid New Password 7        ${BASE PASSWORD}          ${GLYPH TEXT}
Invalid New Password 8        ${BASE PASSWORD}          ${TM TEXT}
Invalid New Password 9        ${BASE PASSWORD}          ${SPACE}${BASE PASSWORD}
Invalid New Password 10       ${BASE PASSWORD}          ${BASE PASSWORD}${SPACE}
Empty New Password            ${BASE PASSWORD}          ${EMPTY}
Empty Both                    ${EMPTY}                  ${EMPTY}

*** Keywords ***
Restart
    Close Browser
    Open Change Password Dialog

Open Change Password Dialog
    Open Browser and go to URL    ${url}/account/password
    Log In    ${EMAIL OWNER}    ${BASE PASSWORD}    None
    Validate Log In
    ######## TEMPORARYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY
    Go To    ${url}/account/password
    Wait Until Element Is Not Visible    ${LOG IN MODAL}
    Wait Until Elements Are Visible    ${CURRENT PASSWORD INPUT}    ${NEW PASSWORD INPUT}    ${CHANGE PASSWORD BUTTON}

Test Passwords Invalid
    [Arguments]    ${old pw}    ${new pw}
    Wait Until Elements Are Visible    ${CURRENT PASSWORD INPUT}    ${NEW PASSWORD INPUT}    ${CHANGE PASSWORD BUTTON}
    Change Password Form Validation    ${old pw}    ${new pw}
    Run Keyword Unless    "${old pw}" == "${BASE PASSWORD}" or "${old pw}" == "${7char password}"    Check Old Password Outline
    Run Keyword Unless    "${new pw}" == "${BASE PASSWORD}"    Check New Password Outline    ${new pw}
    Run Keyword If    "${old pw}" == "${7char password}"    Check Old Password Alert

Change Password Form Validation
    [arguments]    ${old password}    ${new password}
    Input Text    ${CURRENT PASSWORD INPUT}    ${old password}
    Input Text    ${NEW PASSWORD INPUT}    ${new password}
    Click Button    ${CHANGE PASSWORD BUTTON}

Check Old Password Outline
    Wait Until Element Is Visible    ${CURRENT PASSWORD INPUT}/parent::div/parent::div[contains(@class,'has-error')]
    Element Should Be Visible    ${CURRENT PASSWORD IS REQUIRED}

Check Old Password Alert
    Check For Alert    ${CANNOT SAVE PASSWORD} ${PASSWORD INCORRECT}

Check New Password Outline
    [Arguments]    ${new pw}
    Wait Until Element Is Visible    //form[@name='passwordForm']//password-input[@ng-model='pass.newPassword']//input[contains(@class, 'ng-invalid') and @type='password']
    Run Keyword If    "${new pw}"=="${EMPTY}" or "${new pw}"=="${SPACE}"    Element Should Be Visible    ${PASSWORD IS REQUIRED}
    Run Keyword If    "${new pw}"=="${7char password}"    Element Should Be Visible    ${PASSWORD TOO SHORT}
    Run Keyword If    "${new pw}"=="${CYRILLIC TEXT}" or "${new pw}"=="${SMILEY TEXT}" or "${new pw}"=="${GLYPH TEXT}" or "${new pw}"=="${TM TEXT}" or "${new pw}"=="${SPACE}${BASE PASSWORD}" or "${new pw}"=="${BASE PASSWORD}${SPACE}"    Element Should Be Visible    ${PASSWORD SPECIAL CHARS}
    Run Keyword If    "${new pw}"=="${common password}"    Element Should Be Visible    ${PASSWORD TOO COMMON}
    Run Keyword If    "${new pw}"=="${weak password}"    Element Should Be Visible    ${PASSWORD IS WEAK}