*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers
Test Template     Test Register Invalid

*** Variables ***
${url}    ${CLOUD TEST}
${existing email}               ${EMAIL VIEWER}
${no upper password}            adrhartjad
${7char password}               asdfghj
${common password}              yyyyyyyy
${valid email}                  noptixqa+valid@gmail.com

*** Test Cases ***    FIRST       LAST        EMAIL                     PASS
Invalid Email 1       mark        hamill      noptixqagmail.com         ${BASE PASSWORD}
Invalid Email 2       mark        hamill      @gmail.com                ${BASE PASSWORD}
Invalid Email 3       mark        hamill      noptixqa@gmail..com       ${BASE PASSWORD}
Invalid Email 4       mark        hamill      noptixqa@192.168.1.1.0    ${BASE PASSWORD}
Invalid Email 5       mark        hamill      noptixqa.@gmail.com       ${BASE PASSWORD}
Invalid Email 6       mark        hamill      noptixq..a@gmail.c        ${BASE PASSWORD}
Invalid Email 7       mark        hamill      noptixqa@-gmail.com       ${BASE PASSWORD}
Empty Email           mark        hamill      ${EMPTY}                  ${BASE PASSWORD}
Invalid Password 1    mark        hamill      ${EMAIL UNREGISTERED}     ${7char password}
Invalid Password 2    mark        hamill      ${EMAIL UNREGISTERED}     ${no upper password}
Empty Password        mark        hamill      ${EMAIL UNREGISTERED}     ${EMPTY}
Invalid First Name    ${SPACE}    hamill      ${EMAIL UNREGISTERED}     ${BASE PASSWORD}
Empty First Name      ${EMPTY}    hamill      ${EMAIL UNREGISTERED}     ${BASE PASSWORD}
Invalid Last Name     mark        ${SPACE}    ${EMAIL UNREGISTERED}     ${BASE PASSWORD}
Empty Last Name       mark        ${EMPTY}    ${EMAIL UNREGISTERED}     ${BASE PASSWORD}
Invalid All           ${SPACE}    ${SPACE}    noptixqagmail.com         ${7char password}
Empty All             ${EMPTY}    ${EMPTY}    ${EMPTY}                  ${EMPTY}

*** Keywords ***
Test Register Invalid
    [Arguments]    ${first}    ${last}    ${email}    ${pass}
    Open Browser and go to URL    ${url}/register
    Form Validation    Register    ${first}    ${last}    ${email}    ${pass}
    Run Keyword Unless    "${pass}"=="${BASE PASSWORD}"    Check Password Outline
    Run Keyword Unless    "${email}"=="${EMAIL UNREGISTERED}"    Check Email Outline
    Run Keyword Unless    "${first}"=="mark"    Check First Name Outline
    Run Keyword Unless    "${last}"=="hamill"    Check Last Name Outline
    Close Browser

Check Email Outline
    ${class}    Get Element Attribute    ${REGISTER EMAIL INPUT}/../..    class
    Should Contain    ${class}    has-error

Check Password Outline
    ${class}    Get Element Attribute    ${REGISTER PASSWORD INPUT}/../../..    class
    Should Contain    ${class}    has-error

Check First Name Outline
    ${class}    Get Element Attribute    ${REGISTER FIRST NAME INPUT}/../..    class
    Should Contain    ${class}    has-error

Check Last Name Outline
    ${class}    Get Element Attribute    ${REGISTER LAST NAME INPUT}/../..    class
    Should Contain    ${class}    has-error