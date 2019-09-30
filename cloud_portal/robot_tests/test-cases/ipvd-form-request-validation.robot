*** Settings ***
Resource          ../ipvd_resource.robot
Suite Setup       Open Browser and go to URL    ${url}/ipvd
Test Template     Test Submit Request Message
Test Teardown     NONE
Suite Teardown    Close All Browsers
Force Tags        form    Threaded File

*** Variables ***
${url}                  ${ENV}
${name}                 Nx Automated QA
${message}              This is an automated test message.


*** Test Cases ***                   Expect Success     Your Name       Email                  Message       Contact Me       Agree to Privacy Policy
Valid email with all required data        True          ${name}         ${EMAIL OWNER}         ${message}    True             True
    [tags]    C48969    Valid
Valid email but decline Privacy Policy    False         ${name}         ${EMAIL OWNER}         ${message}    False            False
    [tags]    C48969    Valid
Invalid email with all required data 1    False         ${name}         myemail                ${message}    True             True
    [tags]    C48969    Invalid
Invalid email with all required data 2    False         ${name}         myemail@               ${message}    True             True
    [tags]    C48969    Invalid
Invalid email with all required data 3    False         ${name}         myemail@gmail          ${message}    True             True
    [tags]    C48969    Invalid
Invalid email with all required data 4    False         ${name}         my@email@gmail.com     ${message}    True             True
    [tags]    C48969    Invalid
Invalid email with all required data 5    False         ${name}         myemail@ gmail.com     ${message}    True             True
    [tags]    C48969    Invalid
Invalid email with all required data 6    False         ${name}         myemail@ gmail.com$    ${message}    True             True
    [tags]    C48969    Invalid


*** Keywords ***
Test Submit Request Message
    [Arguments]    ${Expect Success}    ${Your Name}    ${Email}    ${Message}    ${Contact Me}    ${Agree to Privacy Policy}
    Go To IPVD page
    Wait Until Element Is Visible    ${SUBMIT A REQUEST}
    Click Element    ${SUBMIT A REQUEST}
    Wait Until Element Is Visible    ${IPVD FEEDBACK}
    Element Text Should Be    ${IPVD FEEDBACK TITLE}    Feedback for cameras page
    Submit Feedback/Request Form    ${Your Name}    ${Email}    ${Message}    ${Contact Me}    ${Agree to Privacy Policy}
    Run Keyword If    ${Expect Success}==True    Validate Message Sent
    ...    ELSE IF    ${Expect Success}==False   Validate Message Not Sent