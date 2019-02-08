*** Settings ***
Resource          ../resource.robot
Suite Setup       Open Browser and go to URL    ${url}/register
Suite Teardown    Close Browser
Test Teardown     Run Keyword If Test Failed    Restart
Test Template     Test Register Invalid
Force Tags        form    Threaded File

*** Variables ***
${url}    ${ENV}
${existing email}              ${EMAIL VIEWER}
${no upper password}           adrhartjad
${7char password}              asdfghj
${symbol password}             pass!@#$%^&*()_-+=;:'"`~,./\|?[]{}
${common password}             qweasd123
${weak password}               asqwerdf
${fair password}               qweasd1234
${valid email}                 noptixqa+valid@gmail.com

*** Test Cases ***                        FIRST       LAST        EMAIL                     PASS                        CHECKED
Invalid Email 1 noptixqagmail.com         mark        hamill      noptixqagmail.com         ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 2 @gmail.com                mark        hamill      @gmail.com                ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 3 noptixqa@gmail..com       mark        hamill      noptixqa@gmail..com       ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 4 noptixqa@192.168.1.1.0    mark        hamill      noptixqa@192.168.1.1.0    ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 5 noptixqa.@gmail.com       mark        hamill      noptixqa.@gmail.com       ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 6 noptixq..a@gmail.c        mark        hamill      noptixq..a@gmail.c        ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 7 noptixqa@-gmail.com       mark        hamill      noptixqa@-gmail.com       ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 8 space                     mark        hamill      ${SPACE}                  ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 9 myemail@                  mark        hamill      myemail@                  ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 10 myemail@gmail            mark        hamill      myemail@gmail             ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 11 myemail@.com             mark        hamill      myemail@.com              ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 12 my@email@gmail.com       mark        hamill      my@email@gmail.com        ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 13 myemail@ gmail.com       mark        hamill      myemail@ gmail.com        ${BASE PASSWORD}            True
    [tags]    C41557
Invalid Email 14 myemail@gmail.com;       mark        hamill      myemail@gmail.com;        ${BASE PASSWORD}            True
    [tags]    C41557
Empty Email                               mark        hamill      ${EMPTY}                  ${BASE PASSWORD}            True
    [tags]    C41556
Registered Email                          mark        hamill      ${existing email}         ${BASE PASSWORD}            True
Short Password asdfghj                    mark        hamill      ${valid email}            ${7char password}           True
    [tags]    C41860
No Uppercase Password adrhartjad          mark        hamill      ${valid email}            ${no upper password}        True
    [tags]    C41860
Common Password qweasd123                 mark        hamill      ${valid email}            ${common password}          True
    [tags]    C41860
Weak Password asqwerdf                    mark        hamill      ${valid email}            ${weak password}            True
    [tags]    C41860
Cyrillic Password Кенгшщзх                mark        hamill      ${valid email}            ${CYRILLIC TEXT}            True
    [tags]    C41860
Smiley Password ☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★      mark        hamill      ${valid email}            ${SMILEY TEXT}              True
    [tags]    C41860
Glyph Password 您都可以享受源源不絕的好禮及優惠    mark        hamill      ${valid email}            ${GLYPH TEXT}               True
    [tags]    C41860
TM Password qweasdzxc123®™                mark        hamill      ${valid email}            ${TM TEXT}                  True
    [tags]    C41860
Leading Space Password                    mark        hamill      ${valid email}            ${SPACE}${BASE PASSWORD}    True
    [tags]    C41860
Trailing Space Password                   mark        hamill      ${valid email}            ${BASE PASSWORD}${SPACE}    True
    [tags]    C41860
Middle Space Password qweasd 123          mark        hamill      ${valid email}            ${BASE PASSWORD}            True
    [tags]    C41862
Empty Password                            mark        hamill      ${valid email}            ${EMPTY}                    True
    [tags]    C41556
Symbol Password pass!@#$%^&*()_-+=;:'"`~,./\|?[]{}    mark        hamill      ${valid email}            ${symbol password}          True
    [tags]    C41861
Invalid First Name                        ${SPACE}    hamill      ${valid email}            ${BASE PASSWORD}            True
Empty First Name                          ${EMPTY}    hamill      ${valid email}            ${BASE PASSWORD}            True
    [tags]    C41556
Invalid Last Name                         mark        ${SPACE}    ${valid email}            ${BASE PASSWORD}            True
Empty Last Name                           mark        ${EMPTY}    ${valid email}            ${BASE PASSWORD}            True
    [tags]    C41556
Invalid All                               ${SPACE}    ${SPACE}    noptixqagmail.com         ${7char password}           True
    [tags]    C41556
Terms Unchecked                           mark        hamill      ${valid email}            ${BASE PASSWORD}            False
    [tags]    C41556
Empty All                                 ${EMPTY}    ${EMPTY}    ${EMPTY}                  ${EMPTY}                    False
    [tags]    C41556

*** Keywords ***
Restart
    Close Browser
    Open Browser and go to URL    ${url}/register

Test Register Invalid
    [Arguments]    ${first}    ${last}    ${email}    ${pass}    ${checked}
    Reload Page
    # These two lines are because Hebrew has double quotes in its text.
    # This makes for issues with strings in xpaths.  These lines convert to single quotes if the language is Hebrew
    Run Keyword If    "${LANGUAGE}"=="he_IL"    Set Suite Variable    ${EMAIL INVALID}    //span[@ng-if="registerForm.registerEmail.$touched && registerForm.registerEmail.$error.email" and contains(text(),'${EMAIL INVALID TEXT}')]
    Run Keyword If    "${LANGUAGE}"=="he_IL"    Set Suite Variable    ${EMAIL IS REQUIRED}    //span[@ng-if="registerForm.registerEmail.$touched && registerForm.registerEmail.$error.required" and contains(text(),'${EMAIL IS REQUIRED TEXT}')]
    Elements Should Not Be Visible    ${EMAIL INVALID}    ${EMAIL ALREADY REGISTERED}    ${EMAIL IS REQUIRED}    ${REGISTER EMAIL INPUT}/parent::div/parent::div[contains(@class,"has-error")]
    ...                               ${PASSWORD BADGE}    ${PASSWORD IS REQUIRED}    ${PASSWORD TOO SHORT}    ${PASSWORD SPECIAL CHARS}    ${PASSWORD TOO COMMON}    ${PASSWORD IS WEAK}    ${REGISTER PASSWORD INPUT}/../input[contains(@class,'ng-invalid ')]
    ...                               ${FIRST NAME IS REQUIRED}    ${REGISTER FIRST NAME INPUT}/parent::div/parent::div[contains(@class,"has-error")]    ${LAST NAME IS REQUIRED}    ${REGISTER LAST NAME INPUT}/parent::div/parent::div[contains(@class,"has-error")]    ${TERMS AND CONDITIONS ERROR}
    Wait Until Elements Are Visible    ${REGISTER FIRST NAME INPUT}    ${REGISTER LAST NAME INPUT}    ${REGISTER EMAIL INPUT}    ${REGISTER PASSWORD INPUT}    ${CREATE ACCOUNT BUTTON}
    Register Form Validation    ${first}    ${last}    ${email}    ${pass}    ${checked}
    Run Keyword Unless    '''${pass}'''=='''${BASE PASSWORD}''' or '''${pass}'''=='''${symbol password}'''    Check Password Outline    ${pass}
    Run Keyword Unless    "${email}"=="${valid email}"    Check Email Outline    ${email}
    Run Keyword Unless    "${first}"=="mark"    Check First Name Outline    ${first}
    Run Keyword Unless    "${last}"=="hamill"    Check Last Name Outline    ${last}
    Run Keyword Unless    "${checked}"=="True"    Check Terms and Conditions Error

Register Form Validation
    [arguments]    ${first name}    ${last name}    ${email}    ${password}    ${checked}
    Input Text    ${REGISTER FIRST NAME INPUT}    ${first name}
    Input Text    ${REGISTER LAST NAME INPUT}    ${last name}
    Input Text    ${REGISTER EMAIL INPUT}    ${email}
    Click Element    ${REGISTER PASSWORD INPUT}
    sleep    .1
    Input Text    ${REGISTER PASSWORD INPUT}    ${password}
    Run Keyword If    '''${password}'''!='''${EMPTY}'''     Check Password Badge    ${password}
    Run Keyword If    "${checked}"=="True"    Click Element    ${TERMS AND CONDITIONS CHECKBOX REAL}
    Sleep    .1    #On Ubuntu it was going too fast
    click button    ${CREATE ACCOUNT BUTTON}

Check Password Badge
    [arguments]    ${pass}
    Wait Until Element Is Visible    ${PASSWORD BADGE}
    Run Keyword If    '''${pass}'''=='''${7char password}'''    Element Should Be Visible    ${PASSWORD TOO SHORT BADGE}
    ...    ELSE IF    '''${pass}'''=='''${no upper password}''' or '''${pass}'''=='''${weak password}'''    Element Should Be Visible    ${PASSWORD IS WEAK BADGE}
    ...    ELSE IF    '''${pass}'''=='''${common password}'''    Element Should Be Visible    ${PASSWORD TOO COMMON BADGE}
    ...    ELSE IF    '''${pass}'''=='''${CYRILLIC TEXT}''' or '''${pass}'''=='''${SMILEY TEXT}''' or '''${pass}'''=='''${GLYPH TEXT}''' or '''${pass}'''=='''${TM TEXT}''' or '''${pass}'''=='''${SPACE}${BASE PASSWORD}''' or '''${pass}'''=='''${BASE PASSWORD}${SPACE}'''    Element Should Be Visible    ${PASSWORD INCORRECT BADGE}
    ...    ELSE IF    '''${pass}'''=='''${fair password}''' or '''${pass}'''=='''${symbol password}'''   Element Should Be Visible    ${PASSWORD IS FAIR BADGE}
    ...    ELSE IF    '''${pass}'''=='''${BASE PASSWORD}'''    Element Should Be Visible    ${PASSWORD IS GOOD BADGE}

Check Email Outline
    [Arguments]    ${email}
    Wait Until Element Is Visible    ${REGISTER EMAIL INPUT}/parent::div/parent::div[contains(@class,"has-error")]
    Run Keyword If    "${email}"=="${EMPTY}" or "${email}"=="${SPACE}"    Element Should Be Visible    ${EMAIL IS REQUIRED}
    Run Keyword If    "${email}"=="${existing email}"    Element Should Be Visible    ${EMAIL ALREADY REGISTERED}
    Run Keyword Unless    "${email}"=="${EMPTY}" or "${email}"=="${SPACE}" or "${email}"=="${existing email}"    Element Should Be Visible    ${EMAIL INVALID}

Check Password Outline
    [Arguments]    ${pass}
    Wait Until Element Is Visible    ${REGISTER PASSWORD INPUT}/../input[contains(@class,'ng-invalid')]
    Run Keyword If    '''${pass}'''=='''${EMPTY}''' or '''${pass}'''=='''${SPACE}'''    Element Should Be Visible    ${PASSWORD IS REQUIRED}
    Run Keyword If    '''${pass}'''=='''${7char password}'''    Element Should Be Visible    ${PASSWORD TOO SHORT}
    Run Keyword If    '''${pass}'''=='''${CYRILLIC TEXT}''' or '''${pass}'''=='''${SMILEY TEXT}''' or '''${pass}'''=='''${GLYPH TEXT}''' or '''${pass}'''=='''${TM TEXT}''' or '''${pass}'''=='''${SPACE}${BASE PASSWORD}''' or '''${pass}'''=='''${BASE PASSWORD}${SPACE}'''    Element Should Be Visible    ${PASSWORD SPECIAL CHARS}
    Run Keyword If    '''${pass}'''=='''${common password}'''    Element Should Be Visible    ${PASSWORD TOO COMMON}
    Run Keyword If    '''${pass}'''=='''${weak password}'''    Element Should Be Visible    ${PASSWORD IS WEAK}

Check First Name Outline
    [Arguments]    ${first}
    Wait Until Element Is Visible    ${REGISTER FIRST NAME INPUT}/parent::div/parent::div[contains(@class,"has-error")]
    Element Should Be Visible    ${FIRST NAME IS REQUIRED}

Check Last Name Outline
    [Arguments]    ${last}
    Wait Until Element Is Visible    ${REGISTER LAST NAME INPUT}/parent::div/parent::div[contains(@class,"has-error")]
    Element Should Be Visible    ${LAST NAME IS REQUIRED}

Check Terms and Conditions Error
    Wait Until Element Is Visible    ${TERMS AND CONDITIONS ERROR}