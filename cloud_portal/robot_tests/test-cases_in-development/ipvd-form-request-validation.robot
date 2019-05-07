*** Settings ***
Resource          ../resource.robot
Suite Setup       NONE
Test Template     Test Submit Request Message
Test Teardown     Close Browser
Suite Teardown    Close All Browsers
Force Tags        form    Threaded File

*** Variables ***
${url}                  ${ENV}
${name}                 Nx Automated QA
${message}              This is an automated test message.


*** Test Cases ***                   Expect Success     Your Name       Email                           Message       Contact Me       Agree to Privacy Policy
Valid email with all required data        True          ${name}         noptixautoqa+owner@gmail.com    ${message}    True             True
    [tags]    C48969
Valid email but decline Privacy Policy    False         ${name}         noptixautoqa+owner@gmail.com    ${message}    False            False
    [tags]    C48969
Invalid email with all required data 1    False         ${name}         myemail                         ${message}    True             True
    [tags]    C48969
Invalid email with all required data 2    False         ${name}         myemail@                        ${message}    True             True
    [tags]    C48969
Invalid email with all required data 3    False         ${name}         myemail@gmail                   ${message}    True             True
    [tags]    C48969
Invalid email with all required data 4    False         ${name}         my@email@gmail.com              ${message}    True             True
    [tags]    C48969
Invalid email with all required data 5    False         ${name}         myemail@ gmail.com              ${message}    True             True
    [tags]    C48969
Invalid email with all required data 6    False         ${name}         myemail@ gmail.com$             ${message}    True             True
    [tags]    C48969


*** Keywords ***
IPVD page
    Open Browser and go to URL    ${url}/ipvd

Restart without login
    Close Browser
    IPVD page

Test Submit Request Message
    [Arguments]    ${Expect Success}    ${Your Name}    ${Email}    ${Message}    ${Contact Me}    ${Agree to Privacy Policy}
    IPVD page
    Wait Until Element Is Visible    ${SUBMIT A REQUEST}
    Click Element    ${SUBMIT A REQUEST}
    Wait Until Element Is Visible    ${IPVD FEEDBACK}
    Submit Request Form    ${Your Name}    ${Email}    ${Message}    ${Contact Me}    ${Agree to Privacy Policy}
    Run Keyword If    ${Expect Success}==True    Validate Message Sent
    Run Keyword If    ${Expect Success}==False    Validate Message Not Sent


Submit Request Form
    [Arguments]    ${Your Name}    ${Email}    ${Message}    ${Contact Me}    ${Agree to Privacy Policy}
    Input Text    ${IPVD FEEDBACK YOUR NAME}    ${Your Name}
    Input Text    ${IPVD FEEDBACK EMAIL}    ${Email}
    Input Text    ${IPVD FEEDBACK MESSAGE}    ${Message}
    Set Checkbox Value    ${IPVD FEEDBACK CONTACT ME}    ${Contact Me}
    Set Checkbox Value    ${IPVD FEEDBACK AGREE}    ${Agree to Privacy Policy}
    Click Button    ${IPVD FEEDBACK SEND BUTTON}
    Sleep    2


Validate Message Sent
    Page Should Not Contain Element    ${IPVD FEEDBACK}
    Page Should Contain Element    ${IPVD FEEDBACK MESSAGE SENT}
    #TODO: Check email and verify submitted data received

Validate Message Not Sent
    Page Should Contain Element    ${IPVD FEEDBACK}
    Validate Input Field State    ${IPVD FEEDBACK EMAIL}/../..    False
    Close Browser