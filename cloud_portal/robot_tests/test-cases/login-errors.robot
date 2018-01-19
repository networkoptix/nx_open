*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers
Test Template     Test Login Invalid

*** Variables ***
${url}    ${CLOUD TEST}
${good email}                   noptixqa+viewer@gmail.com
${good email unregistered}      unregistered@notsignedup.com
${good password}                qweasd123
${invalid}                      aderljkadergoij

*** Test Cases ***                EMAIL                         PASS                 EXPECTED
Invalid User Name                 ${invalid}                    ${good password}     'outline'
Invalid Password                  ${good email}                 ${invalid}           'alert'
Invalid User Name and Password    ${invalid}                    ${invalid}           'outline'
Valid Email Unregistered          ${good email unregistered}    ${good password}     'alert'
Empty User Name                   ${EMPTY}                      ${good password}     'outline'
Empty Password                    ${good email}                 ${EMPTY}             'outline'
Empty User Name and Password      ${EMPTY}                      ${EMPTY}             'outline'
Valid User Name and password      ${good email}                 ${good password}     'neither'

*** Keywords ***
Test Login Invalid
    [Arguments]    ${email}    ${pass}    ${expected}
    Open Browser and go to URL    ${url}
    Login    ${email}    ${pass}    ${expected}

Login
    [Arguments]    ${email}    ${pass}    ${expected}
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Click Link    ${LOG IN NAV BAR}
    Wait Until Element Is Visible    ${EMAIL INPUT}
    input text    ${EMAIL INPUT}    ${email}
    Wait Until Element Is Visible    ${PASSWORD INPUT}
    input text    ${PASSWORD INPUT}    ${pass}
    Wait Until Element Is Visible    ${LOG IN BUTTON}
    click button    ${LOG IN BUTTON}
    Run Keyword If    ${expected} == 'outline'    Outline Error    ${email}    ${pass}
    Run Keyword If    ${expected} == 'alert'    Alert Error
    Run Keyword If    ${expected} == 'neither'    Validate Login
    Close Browser

Outline Error
    [Arguments]    ${email}    ${pass}
    Run Keyword If    "${pass}" == "${EMPTY}"    Check Password Outline
    Run Keyword If    "${email}" == "${EMPTY}" or "${email}" == "${invalid}"    Check Email Outline

Alert Error
    wait until element is visible    ${PASSWORD INPUT}
    Element Should Be Visible    ${PASSWORD INPUT}

Check Email Outline
    ${class}    Get Element Attribute    ${EMAIL INPUT}/..    class
    Should Contain    ${class}    has-error

Check Password Outline
    ${class}    Get Element Attribute    ${PASSWORD INPUT}/..    class
    Should Contain    ${class}    has-error
