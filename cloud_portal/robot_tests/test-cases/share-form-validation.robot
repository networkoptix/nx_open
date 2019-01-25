*** Settings ***
Resource          ../resource.robot
Suite Setup       Open Share Dialog
Suite Teardown    Close Browser
Test Teardown     Run Keyword If Test Failed    Restart
Test Template     Test Email Invalid
Force Tags        email    form    Threaded File

*** Variables ***
${url}    ${ENV}
${password}     ${BASE PASSWORD}
${email}    ${EMAIL OWNER}

*** Test Cases ***      EMAIL
Empty Email                               ${EMPTY}
    [tags]    C41888
Invalid Email 1 noptixqagmail.com         noptixqagmail.com
    [tags]    C41902
Invalid Email 2 @gmail.com                @gmail.com
    [tags]    C41902
Invalid Email 3 noptixqa@gmail..com       noptixqa@gmail..com
    [tags]    C41902
Invalid Email 4 noptixqa@192.168.1.1.0    noptixqa@192.168.1.1.0
    [tags]    C41902
Invalid Email 5 noptixqa.@gmail.com       noptixqa.@gmail.com
    [tags]    C41902
Invalid Email 6 noptixq..a@gmail.c        noptixq..a@gmail.c
    [tags]    C41902
Invalid Email 7 noptixqa@-gmail.com       noptixqa@-gmail.com
    [tags]    C41902
Invalid Email 8 myemail                   myemail
    [tags]    C41902
Invalid Email 9 myemail@                  myemail@
    [tags]    C41902
Invalid Email 10 myemail@gmail            myemail@gmail
    [tags]    C41902
Invalid Email 11 myemail@.com             myemail@.com
    [tags]    C41902
Invalid Email 12 my@email@gmail.com       my@email@gmail.com
    [tags]    C41902
Invalid Email 13 myemail@ gmail.com       myemail@ gmail.com
    [tags]    C41902
Invalid Email 14 myemail@gmail.com;       myemail@gmail.com;
    [tags]    C41902
Space Email                               ${SPACE}
Leading Space Email                       ${SPACE}myemail@gmail.com
    [tags]    C47296
Trailing Space Email                      myemail@gmail.com${SPACE}
    [tags]    C47296
Valid Email                               myemail@gmail.com
    [tags]    C47296

*** Keywords ***
Restart
    Close Browser
    Open Share Dialog

Open Share Dialog
    Open Browser and go to URL    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Log In    ${email}    ${password}    None
    Validate Log In
    Run Keyword If    '${email}' == '${EMAIL OWNER}'    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}    //div[@ng-if='gettingSystem.success']//h2[@class='card-title']
    Run Keyword If    '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}    //div[@ng-if='gettingSystem.success']
    Run Keyword Unless    '${email}' == '${EMAIL OWNER}' or '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${OPEN IN NX BUTTON}
    Wait Until Element Is Enabled    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Element Is Visible    ${SHARE BUTTON MODAL}

Test Email Invalid
    [Arguments]   ${email}
    Wait Until Element Is Visible    ${SHARE EMAIL}
    Input Text    ${SHARE EMAIL}    ${email}
    Click Element    ${SHARE MODAL}
    Run Keyword If    '${email}' == '${EMPTY}'    Click Button    ${SHARE BUTTON MODAL}
    Run Keyword Unless    '${email}' == '${SPACE}myemail@gmail.com' or '${email}' == 'myemail@gmail.com${SPACE}' or '${email}' == 'myemail@gmail.com'     Wait Until Element Is Visible    //form[@name='shareForm']//input[@id='email' and contains(@class,"ng-invalid")]
    Run Keyword If    '${email}' == '${SPACE}myemail@gmail.com' or '${email}' == 'myemail@gmail.com${SPACE}' or '${email}' == 'myemail@gmail.com'     Wait Until Element Is Visible    //form[@name='shareForm']//input[@id='email' and contains(@class,"ng-valid")]