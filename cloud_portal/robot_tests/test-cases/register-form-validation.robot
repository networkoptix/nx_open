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
Invalid Email 1       None        None        noptixqagmail.com         None
Invalid Email 2       None        None        @gmail.com                None
Invalid Email 3       None        None        noptixqa@gmail..com       None
Invalid Email 4       None        None        noptixqa@192.168.1.1.0    None
Invalid Email 5       None        None        noptixqa.@gmail.com       None
Invalid Email 6       None        None        noptixq..a@gmail.c        None
Invalid Email 7       None        None        noptixqa@-gmail.com       None
Empty Email           None        None        ${EMPTY}                  None
Invalid Password 1    None        None        None                      ${7char password}
Invalid Password 2    None        None        None                      ${no upper password}
Empty Password        None        None        None                      ${EMPTY}
Invalid First Name    ${SPACE}    None        None                      None
Empty First Name      ${EMPTY}    None        None                      None
Invalid Last Name     None        ${SPACE}    None                      None
Empty Last Name       None        ${EMPTY}    None                      None
Invalid All           ${SPACE}    ${SPACE}    noptixqagmail.com         ${7char password}
Empty All             ${EMPTY}    ${EMPTY}    ${EMPTY}                  ${EMPTY}

*** Keywords ***
Test Register Invalid
    [Arguments]    ${first}    ${last}    ${email}    ${pass}
    Open Browser and go to URL    ${url}/register
    Form Validation    Register    ${first}    ${last}    ${email}    ${pass}
    Run Keyword Unless    "${pass}"=="None"    Check Password Outline
    Run Keyword Unless    "${email}"=="None"    Check Email Outline
    Run Keyword Unless    "${first}"=="None"    Check First Name Outline
    Run Keyword Unless    "${last}"=="None"    Check Last Name Outline
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