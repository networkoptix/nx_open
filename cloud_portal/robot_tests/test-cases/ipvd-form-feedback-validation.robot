*** Settings ***
Resource          ../ipvd_resource.robot
Suite Setup       Open IPVD page and Log In
Test Template     Test Submit Feedback Message
Test Teardown     NONE
Suite Teardown    Close All Browsers
Force Tags        form    Threaded File

*** Variables ***
${url}                  ${ENV}
${name}                 Nx Automated QA
${message}              This is an automated test message.


*** Test Cases ***                   Expect Success     Your Name       Email                  Message       Contact Me       Agree to Privacy Policy
Valid email with all required data        True          ${name}         ${EMAIL OWNER}         ${message}    True             True
    [tags]    C54182    Valid
Valid email but decline Privacy Policy    False         ${name}         ${EMAIL OWNER}         ${message}    False            False
    [tags]    C54182    Valid
Invalid email with all required data 1    False         ${name}         myemail                ${message}    True             True
    [tags]    C54182    Invalid
Invalid email with all required data 2    False         ${name}         myemail@               ${message}    True             True
    [tags]    C54182    Invalid
Invalid email with all required data 3    False         ${name}         myemail@gmail          ${message}    True             True
    [tags]    C54182    Invalid
Invalid email with all required data 4    False         ${name}         my@email@gmail.com     ${message}    True             True
    [tags]    C54182    Invalid
Invalid email with all required data 5    False         ${name}         myemail@ gmail.com     ${message}    True             True
    [tags]    C54182    Invalid
Invalid email with all required data 6    False         ${name}         myemail@ gmail.com$    ${message}    True             True
    [tags]    C54182    Invalid


*** Keywords ***
Test Submit Feedback Message
    [Arguments]    ${Expect Success}    ${Your Name}    ${Email}    ${Message}    ${Contact Me}    ${Agree to Privacy Policy}
    Go To IPVD page
    #Search for Axis and click any camera from list
    IPVD Text Search    Axis
    IPVD Select Device From Table Randomly
    Wait Until Element Is Visible    ${SEND DEVICE FEEDBACK}
    Click Element    ${SEND DEVICE FEEDBACK}
    Wait Until Element Is Visible    ${IPVD FEEDBACK}
    ${model}=   Get Text    ${IPVD DEVICE MODEL}
    Element Should Contain    ${IPVD FEEDBACK TITLE}    Feedback about ${model}
    Submit Feedback/Request Form    ${Your Name}    ${Email}    ${Message}    ${Contact Me}    ${Agree to Privacy Policy}
    Run Keyword If    ${Expect Success}==True    Validate Message Sent
    ...    ELSE IF    ${Expect Success}==False   Validate Message Not Sent