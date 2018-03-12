*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers
Test Template     Test Login Invalid

*** Variables ***
${url}    ${ENV}
${good email}                   ${EMAIL VIEWER}
${good email unregistered}      ${EMAIL UNREGISTERED}
${good password}                ${BASE PASSWORD}
${bad password}                 adrhartjad

*** Test Cases ***            EMAIL                         PASS                EXPECTED
Invalid Email 1               noptixqagmail.com             ${good password}    outline
Invalid Email 2               @gmail.com                    ${good password}    outline
Invalid Email 3               noptixqa@gmail..com           ${good password}    outline
Invalid Email 4               noptixqa@192.168.1.1.0        ${good password}    outline
Invalid Email 5               noptixqa.@gmail.com           ${good password}    outline
Invalid Email 6               noptixq..a@gmail.c            ${good password}    outline
Invalid Email 7               noptixqa@-gmail.com           ${good password}    outline
Invalid Password              ${good email}                 ${bad password}     alert
Invalid Email and Password    noptixqagmail.com             ${bad password}     outline
Valid Email Unregistered      ${good email unregistered}    ${good password}    alert
Empty Email                   ${EMPTY}                      ${good password}    outline
Empty Password                ${good email}                 ${EMPTY}            outline
Empty Email and Password      ${EMPTY}                      ${EMPTY}            outline
Valid Email and password      ${good email}                 ${good password}    neither

*** Keywords ***
Test Login Invalid
    [Arguments]    ${email}    ${pass}    ${expected}
    Open Browser and go to URL    ${url}
    Form Validation    Log In    email=${email}    password=${pass}
    Run Keyword If    "${expected} "== "outline"    Outline Error    ${email}    ${pass}
    Run Keyword If    "${expected}" == "alert"    Alert Error    ${email}    ${pass}
    Run Keyword If    "${expected}" == "neither"    Validate Login
    Close Browser

Outline Error
    [Arguments]    ${email}    ${pass}
    Run Keyword If    "${pass}" == "${EMPTY}"    Check Password Outline
    Run Keyword Unless    "${email}" == "${good email}" or "${email}" == "${good email unregistered}"    Check Email Outline

Alert Error
    [arguments]    ${email}    ${pass}
    Run Keyword If    "${email}" == "${good email unregistered}"    Check For Alert    ${ACCOUNT DOES NOT EXIST}
    Run Keyword If    "${pass}" == "${bad password}"    Check For Alert    ${WRONG PASSWORD}

Check Email Outline
    ${class}    Get Element Attribute    ${EMAIL INPUT}/..    class
    Should Contain    ${class}    has-error

Check Password Outline
    ${class}    Get Element Attribute    ${PASSWORD INPUT}/..    class
    Should Contain    ${class}    has-error
