*** Settings ***
Resource          ../resource.robot
Test Teardown     Restart
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers

*** Variables ***
${email}           ${EMAIL OWNER}
${password}        ${BASE PASSWORD}
${url}             ${ENV}

*** Keywords ***
Restart
    Close Browser
    Open Browser and go to URL    ${url}

*** Test Cases ***
About page is correctly displayed
    [tags]    C41541    Threaded
    Wait Until Elements Are Visible    ${ABOUT CLOUD NAME}    ${CREATE ACCOUNT BODY}    ${FOOTER ABOUT LINK}
    Wait Until Element Has Style    ${CREATE ACCOUNT BODY}    background-color    ${THEME COLOR RGB}
    Click Link    ${FOOTER ABOUT LINK}
    Location Should Be    ${ENV}${ABOUT URL}
    Wait Until Elements Are Visible    ${ABOUT CLOUD NAME}    ${CREATE ACCOUNT BODY}    ${FOOTER ABOUT LINK}
    Wait Until Element Has Style    ${CREATE ACCOUNT BODY}    background-color    ${THEME COLOR RGB}

Support leads to the proper support site
    [tags]    C41544    Threaded
    Wait Until Element Is Visible    ${FOOTER SUPPORT LINK}
    Click Link    ${FOOTER SUPPORT LINK}
    ${tabs}    Get Window Handles
    Select Window    @{tabs}[1]
    Location Should Contain    ${SUPPORT URL}

Terms leads to the proper EULA site
    [tags]    C41545    Threaded
    Wait Until Element Is Visible    ${FOOTER TERMS LINK}
    Click Link    ${FOOTER TERMS LINK}
    Location Should Be    ${ENV}${TERMS URL}

Privacy leads to the proper page
    [tags]    C41546    Threaded
    Wait Until Element Is Visible    ${FOOTER PRIVACY LINK}
    Click Link    ${FOOTER PRIVACY LINK}
    Location Should Be    ${PRIVACY POLICY URL}

Copyright leads to the proper site
    [tags]    C41547    Threaded
    Wait Until Element Is Visible    ${FOOTER COPYRIGHT LINK}
    Click Link    ${FOOTER COPYRIGHT LINK}
    ${tabs}    Get Window Handles
    Select Window    @{tabs}[1]
    Location Should Be    ${COPYRIGHT URL}

Change interface language
    [tags]    C41549
    :FOR    ${lang}    ${account}   IN ZIP    ${LANGUAGES LIST}    ${LANGUAGES CREATE ACCOUNT TEXT LIST}
    \  Sleep    2
    \  Run Keyword Unless    "${lang}"=="${LANGUAGE}"    Wait Until Element Is Visible    ${LANGUAGE DROPDOWN}
    \  Run Keyword Unless    "${lang}"=="${LANGUAGE}"    Click Button    ${LANGUAGE DROPDOWN}
    \  Run Keyword Unless    "${lang}"=="${LANGUAGE}"    Wait Until Element Is Visible    //nx-footer//span[@lang='${lang}']/..
    \  Run Keyword Unless    "${lang}"=="${LANGUAGE}"    Click Element    //nx-footer//span[@lang='${lang}']/..
    \  Sleep    2    #to allow the system to change languages
    \  Run Keyword Unless    "${lang}"=="${LANGUAGE}"    Wait Until Element Is Visible    //h1['${account}']
    Wait Until Element Is Visible    ${LANGUAGE DROPDOWN}
    Click Button    ${LANGUAGE DROPDOWN}
    Wait Until Element Is Visible    //nx-footer//span[@lang='${lang}']/..
    Click Element    //nx-footer//span[@lang='${lang}']/..
    Sleep    1
    Wait Until Element Is Visible    //h1['${ACCOUNT TEXT}']