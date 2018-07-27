*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Setup       Open Restore Password Dialog With Link
Suite Teardown    Close Browser
Test Teardown     Run Keyword If Test Failed    Restart
Test Template     Test Password Invalid
Force Tags        email    form

*** Variables ***
${url}    ${ENV}
${password}    ${BASE PASSWORD}
${no upper password}           adrhartjad
${7char password}              asdfghj
${common password}             yyyyyyyy
${weak password}               asqwerdf

${PASSWORD IS REQUIRED}            //span[@ng-if='passwordInput.password.$error.required' and contains(text(),'${PASSWORD IS REQUIRED TEXT}')]
${PASSWORD SPECIAL CHARS}          //span[@ng-if='passwordInput.password.$error.pattern' and contains(text(),'${PASSWORD SPECIAL CHARS TEXT}')]
${PASSWORD TOO SHORT}              //span[contains(@ng-if,'passwordInput.password.$error.minlength &&') and contains(text(),'${PASSWORD TOO SHORT TEXT}')]
${PASSWORD TOO COMMON}             //span[contains(@ng-if,'form.passwordNew.$error.common &&') and contains(@ng-if,'!form.passwordNew.$error.required') and contains(text(),'${PASSWORD TOO COMMON TEXT}')]
${PASSWORD IS WEAK}                //span[contains(@ng-if,'form.passwordNew.$error.weak &&') and contains(@ng-if,'!form.passwordNew.$error.common &&') and contains(@ng-if,'!form.passwordNew.$error.pattern &&') and contains(@ng-if,'!form.passwordNew.$error.required &&') and contains(@ng-if,'!form.passwordNew.$error.minlength') and contains(text(),'${PASSWORD IS WEAK TEXT}')]

*** Test Cases ***            NEW PW
Invalid New Password 1        ${7char password}
Invalid New Password 2        ${no upper password}
Invalid New Password 3        ${common password}
Invalid New Password 4        ${weak password}
Invalid New Password 5        ${CYRILLIC TEXT}
Invalid New Password 6        ${SMILEY TEXT}
Invalid New Password 7        ${GLYPH TEXT}
Invalid New Password 8        ${TM TEXT}
Invalid New Password 9        ${SPACE}${BASE PASSWORD}
Invalid New Password 10       ${BASE PASSWORD}${SPACE}
Empty New Password            ${EMPTY}

*** Keywords ***
Restart
    Close Browser
    Open Restore Password Dialog With Link

Open Restore Password Dialog With Link
    ${email}    Get Random Email    ${BASE EMAIL}
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    ${link}    Get Email Link    ${email}    activate
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    ${link}    Get Email Link    ${email}    restore_password
    Go To    ${link}
    Wait Until Elements Are Visible    ${RESET PASSWORD INPUT}    ${SAVE PASSWORD}

Test Password Invalid
    [Arguments]   ${new pw}
    Input Text    ${RESET PASSWORD INPUT}    ${new pw}
    Click Button    ${SAVE PASSWORD}
    Check New Password Outline    ${new pw}

Check New Password Outline
    [Arguments]   ${new pw}
    Wait Until Element Is Visible    ${RESET PASSWORD INPUT}/parent::div/parent::div/parent::div[contains(@class,'has-error')]
    Run Keyword If    "${new pw}"=="${EMPTY}" or "${new pw}"=="${SPACE}"    Element Should Be Visible    ${PASSWORD IS REQUIRED}
    Run Keyword If    "${new pw}"=="${7char password}"    Element Should Be Visible    ${PASSWORD TOO SHORT}
    Run Keyword If    "${new pw}"=="${CYRILLIC TEXT}" or "${new pw}"=="${SMILEY TEXT}" or "${new pw}"=="${GLYPH TEXT}" or "${new pw}"=="${TM TEXT}" or "${new pw}"=="${SPACE}${BASE PASSWORD}" or "${new pw}"=="${BASE PASSWORD}${SPACE}"    Element Should Be Visible    ${PASSWORD SPECIAL CHARS}
    Run Keyword If    "${new pw}"=="${common password}"    Element Should Be Visible    ${PASSWORD TOO COMMON}
    Run Keyword If    "${new pw}"=="${weak password}"    Element Should Be Visible    ${PASSWORD IS WEAK}