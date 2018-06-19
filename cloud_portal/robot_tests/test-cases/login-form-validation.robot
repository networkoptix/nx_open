*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close Browser
Test Teardown     Run Keyword If Test Failed    Restart
Test Template     Test Login Invalid
Force Tags        form

*** Variables ***
${url}    ${ENV}
${good email}                   ${EMAIL VIEWER}
${good email unregistered}      ${EMAIL UNREGISTERED}
${good password}                ${BASE PASSWORD}
${bad password}                 adrhartjad

*** Test Cases ***            EMAIL                         PASS
Empty Email                   ${EMPTY}                      ${good password}
Empty Password                ${good email}                 ${EMPTY}
Empty Email and Password      ${EMPTY}                      ${EMPTY}
Invalid Email 1               noptixqagmail.com             ${good password}
Invalid Email 2               @gmail.com                    ${good password}
Invalid Email 3               noptixqa@gmail..com           ${good password}
Invalid Email 4               noptixqa@192.168.1.1.0        ${good password}
Invalid Email 5               noptixqa.@gmail.com           ${good password}
Invalid Email 6               noptixq..a@gmail.c            ${good password}
Invalid Email 7               noptixqa@-gmail.com           ${good password}
Invalid Password              ${good email}                 ${bad password}
Invalid Email and Password    noptixqagmail.com             ${bad password}
Valid Email Unregistered      ${good email unregistered}    ${good password}

*** Keywords ***
Restart
    Close Browser
    Open Log In Dialog

Open Log In Dialog
    Open Browser and go to URL    ${url}

Test Login Invalid
    [Arguments]    ${email}    ${pass}
    Reload Page
    Wait Until Elements Are Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Elements Are Visible    ${EMAIL INPUT}    ${PASSWORD INPUT}    ${LOG IN BUTTON}
    Log In Form Validation    ${email}    ${pass}
    Outline Error    ${email}    ${pass}

Log In Form Validation
    [Arguments]    ${email}    ${pass}
    Input Text    ${EMAIL INPUT}    ${email}
    Input Text    ${PASSWORD INPUT}    ${pass}
    click button    ${LOG IN BUTTON}

Outline Error
    [Arguments]    ${email}    ${pass}
    Run Keyword If    "${pass}" == "${EMPTY}"    Check Password Outline
    Run Keyword Unless    "${email}" == "${good email}" or "${email}" == "${good email unregistered}"    Check Email Outline

Check Email Outline
    ${class}    Get Element Attribute    ${EMAIL INPUT}    class
    Should Contain    ${class}    ng-invalid

Check Password Outline
    ${class}    Get Element Attribute    ${PASSWORD INPUT}    class
    Should Contain    ${class}    ng-invalid
