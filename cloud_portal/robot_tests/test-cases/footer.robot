*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Setup        Restart
#Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers

*** Variables ***
${email}           ${EMAIL OWNER}
${password}        ${BASE PASSWORD}
${url}             ${ENV}

*** Keywords ***
Restart
    Go To    ${url}
*** Test Cases ***
About page is correctly displayed
    [tags]    C41541
    Wait Until Elements Are Visible    ${ABOUT CLOUD NAME}    ${CREATE ACCOUNT BODY}    ${FOOTER ABOUT LINK}
    Wait Until Element Has Style    ${CREATE ACCOUNT BODY}    background-color    ${THEME COLOR RGB}
    Click Link    ${FOOTER ABOUT LINK}
    Location Should Be    ${ABOUT URL}
    Wait Until Elements Are Visible    ${ABOUT CLOUD NAME}    ${CREATE ACCOUNT BODY}    ${FOOTER ABOUT LINK}
    Wait Until Element Has Style    ${CREATE ACCOUNT BODY}    background-color    ${THEME COLOR RGB}

Known limitations". Support link is clickable and lead to the proper site
    [tags]    C41543
    Wait Until Element Is Visible    ${FOOTER KNOWN LIMITS LINK}
    Click Link    ${FOOTER KNOWN LIMITS LINK}
    Location Should Be    ${KNOWN LIMITATIONS URL}

Support leads to the proper support site
    [tags]    C41544
    Wait Until Element Is Visible    ${FOOTER SUPPORT LINK}
    Click Link    ${FOOTER SUPPORT LINK}
    ${tabs}    Get Window Handles
    Select Window    @{tabs}[1]
    Location Should Contain    ${SUPPORT URL}

Terms leads to the proper EULA site
    [tags]    C41545
    Wait Until Element Is Visible    ${FOOTER TERMS LINK}
    Click Link    ${FOOTER TERMS LINK}
    Location Should Be    ${TERMS URL}

Privacy leads to the proper page
    [tags]    C41546
    Wait Until Element Is Visible    ${FOOTER PRIVACY LINK}
    Click Link    ${FOOTER PRIVACY LINK}
    Location Should Be    ${PRIVACY POLICY URL}

Copyright leads to the proper site
    [tags]    C41547
    Wait Until Element Is Visible    ${FOOTER COPYWRIGHT LINK}
    Click Link    ${FOOTER COPYWRIGHT LINK}
    ${tabs}    Get Window Handles
    Select Window    @{tabs}[2]
    Location Should Be    ${COPYWRIGHT URL}

Change interface language
    Click Button    ${LANGUAGE DROPDOWN}
    Wait Until Element Is Visible    //nx-footer//span[@lang='ru_RU']/..
    Click Element    //nx-footer//span[@lang='ru_RU']/..
    Wait Until Element Is Visible    ${LANGUAGE DROPDOWN}/span[@lang='ru_RU']    5
    Check Language