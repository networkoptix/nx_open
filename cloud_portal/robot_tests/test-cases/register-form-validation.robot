*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers
Test Template     Test Register Invalid

*** Variables ***
${url}    ${ENV}
${existing email}              ${EMAIL VIEWER}
${no upper password}           adrhartjad
${7char password}              asdfghj
${common password}             yyyyyyyy
${weak password}               asqwerdf
${valid email}                 noptixqa+valid@gmail.com
#Register form errors
${FIRST NAME IS REQUIRED}      //span[@ng-if='registerForm.firstName.$touched && registerForm.firstName.$error.required' and contains(text(),'${FIRST NAME IS REQUIRED TEXT}')]
${LAST NAME IS REQUIRED}       //span[@ng-if='registerForm.lastName.$touched && registerForm.lastName.$error.required' and contains(text(),'${LAST NAME IS REQUIRED TEXT}')]
${EMAIL IS REQUIRED}           //span[@ng-if='registerForm.registerEmail.$touched && registerForm.registerEmail.$error.required' and contains(text(),'${EMAIL IS REQUIRED TEXT}')]
${EMAIL ALREADY REGISTERED}    //span[@ng-if='registerForm.registerEmail.$error.alreadyExists' and contains(text(),'${EMAIL ALREADY REGISTERED TEXT}')]
${EMAIL INVALID}               //span[@ng-if='registerForm.registerEmail.$touched && registerForm.registerEmail.$error.email' and contains(text(),'${EMAIL INVALID TEXT}')]
${PASSWORD IS REQUIRED}        //span[@ng-if='passwordInput.password.$error.required' and contains(text(),'${PASSWORD IS REQUIRED TEXT}')]
${PASSWORD SPECIAL CHARS}      //span[@ng-if='passwordInput.password.$error.pattern' and contains(text(),'${PASSWORD SPECIAL CHARS TEXT}')]
${PASSWORD TOO SHORT}          //span[contains(@ng-if,'passwordInput.password.$error.minlength &&') and contains(@ng-if,'!passwordInput.password.$error.pattern') and contains(text(),'${PASSWORD TOO SHORT TEXT}')]
${PASSWORD TOO COMMON}         //span[contains(@ng-if,'passwordInput.password.$error.common &&') and contains(@ng-if,'!passwordInput.password.$error.pattern &&') and contains(@ng-if,'!passwordInput.password.$error.required') and contains(text(),'${PASSWORD TOO COMMON TEXT}')]
${PASSWORD IS WEAK}            //span[contains(@ng-if,'passwordInput.password.$error.common &&') and contains(@ng-if,'!passwordInput.password.$error.pattern &&') and contains(@ng-if,'!passwordInput.password.$error.required') and contains(text(),'${PASSWORD IS WEAK TEXT}')]

*** Test Cases ***      FIRST       LAST        EMAIL                     PASS
Invalid Email 1        mark        hamill      noptixqagmail.com         ${BASE PASSWORD}
Invalid Email 2        mark        hamill      @gmail.com                ${BASE PASSWORD}
Invalid Email 3        mark        hamill      noptixqa@gmail..com       ${BASE PASSWORD}
Invalid Email 4        mark        hamill      noptixqa@192.168.1.1.0    ${BASE PASSWORD}
Invalid Email 5        mark        hamill      noptixqa.@gmail.com       ${BASE PASSWORD}
Invalid Email 6        mark        hamill      noptixq..a@gmail.c        ${BASE PASSWORD}
Invalid Email 7        mark        hamill      noptixqa@-gmail.com       ${BASE PASSWORD}
Invalid Email 8        mark        hamill      ${SPACE}                  ${BASE PASSWORD}
Empty Email            mark        hamill      ${EMPTY}                  ${BASE PASSWORD}
Registered Email       mark        hamill      ${existing email}         ${BASE PASSWORD}
Invalid Password 1     mark        hamill      ${valid email}            ${7char password}
Invalid Password 2     mark        hamill      ${valid email}            ${no upper password}
Invalid Password 3     mark        hamill      ${valid email}            ${common password}
Invalid Password 4     mark        hamill      ${valid email}            ${weak password}
Invalid Password 5     mark        hamill      ${valid email}            ${CYRILLIC TEXT}
Invalid Password 6     mark        hamill      ${valid email}            ${SMILEY TEXT}
Invalid Password 7     mark        hamill      ${valid email}            ${GLYPH TEXT}
Invalid Password 8     mark        hamill      ${valid email}            ${TM TEXT}
Invalid Password 9     mark        hamill      ${valid email}            ${SPACE}${BASE PASSWORD}
Invalid Password 10    mark        hamill      ${valid email}            ${BASE PASSWORD}${SPACE}
Empty Password         mark        hamill      ${valid email}            ${EMPTY}
Invalid First Name     ${SPACE}    hamill      ${valid email}            ${BASE PASSWORD}
Empty First Name       ${EMPTY}    hamill      ${valid email}            ${BASE PASSWORD}
Invalid Last Name      mark        ${SPACE}    ${valid email}            ${BASE PASSWORD}
Empty Last Name        mark        ${EMPTY}    ${valid email}            ${BASE PASSWORD}
Invalid All            ${SPACE}    ${SPACE}    noptixqagmail.com         ${7char password}
Empty All              ${EMPTY}    ${EMPTY}    ${EMPTY}                  ${EMPTY}

*** Keywords ***
Test Register Invalid
    [Arguments]    ${first}    ${last}    ${email}    ${pass}
    Open Browser and go to URL    ${url}/register
    Form Validation    Register    ${first}    ${last}    ${email}    ${pass}
    Run Keyword Unless    "${pass}"=="${BASE PASSWORD}"    Check Password Outline    ${pass}
    Run Keyword Unless    "${email}"=="${valid email}"    Check Email Outline    ${email}
    Run Keyword Unless    "${first}"=="mark"    Check First Name Outline    ${first}
    Run Keyword Unless    "${last}"=="hamill"    Check Last Name Outline    ${last}
    Close Browser

Check Email Outline
    [Arguments]    ${email}
    Wait Until Element Is Visible    ${REGISTER EMAIL INPUT}/parent::div/parent::div[contains(@class,'has-error')]
    Run Keyword If    "${email}"=="${EMPTY}" or "${email}"=="${SPACE}"    Element Should Be Visible    ${EMAIL IS REQUIRED}
    Run Keyword If    "${email}"=="${existing email}"    Element Should Be Visible    ${EMAIL ALREADY REGISTERED}
    Run Keyword Unless    "${email}"=="${EMPTY}" or "${email}"=="${SPACE}" or "${email}"=="${existing email}"    Element Should Be Visible    ${EMAIL INVALID}

Check Password Outline
    [Arguments]    ${pass}
    Wait Until Element Is Visible    ${REGISTER PASSWORD INPUT}/parent::div/parent::div/parent::div[contains(@class,'has-error')]
    Run Keyword If    "${pass}"=="${EMPTY}" or "${pass}"=="${SPACE}"    Element Should Be Visible    ${PASSWORD IS REQUIRED}
    Run Keyword If    "${pass}"=="${7char password}"    Element Should Be Visible    ${PASSWORD TOO SHORT}
    Run Keyword If    "${pass}"=="${CYRILLIC TEXT}" or "${pass}"=="${SMILEY TEXT}" or "${pass}"=="${GLYPH TEXT}" or "${pass}"=="${TM TEXT}" or "${pass}"=="${SPACE}${BASE PASSWORD}" or "${pass}"=="${BASE PASSWORD}${SPACE}"    Element Should Be Visible    ${PASSWORD SPECIAL CHARS}
    Run Keyword If    "${pass}"=="${common password}"    Element Should Be Visible    ${PASSWORD TOO COMMON}
    Run Keyword If    "${pass}"=="${weak password}"    Element Should Be Visible    ${PASSWORD IS WEAK}

Check First Name Outline
    [Arguments]    ${first}
    Wait Until Element Is Visible    ${REGISTER FIRST NAME INPUT}/parent::div/parent::div[contains(@class,'has-error')]
    Element Should Be Visible    ${FIRST NAME IS REQUIRED}

Check Last Name Outline
    [Arguments]    ${last}
    Wait Until Element Is Visible    ${REGISTER LAST NAME INPUT}/parent::div/parent::div[contains(@class,'has-error')]
    Element Should Be Visible    ${LAST NAME IS REQUIRED}