*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Setup       Open Restore Password Dialog
Suite Teardown    Close Browser
Test Teardown     Run Keyword If Test Failed    Restart
Test Template     Test Email Invalid
Force Tags        email    form

*** Variables ***
${url}    ${ENV}
${password}     ${BASE PASSWORD}
${EMAIL IS REQUIRED}           //span[@ng-if='restorePassword.email.$touched && restorePassword.email.$error.required' and contains(text(),"${EMAIL IS REQUIRED TEXT}")]
${EMAIL INVALID}               //span[@ng-if='restorePassword.email.$touched && restorePassword.email.$error.email' and contains(text(),"${EMAIL INVALID TEXT}")]

*** Test Cases ***      EMAIL
Invalid Email 1         noptixqagmail.com
Invalid Email 2         @gmail.com
Invalid Email 3         noptixqa@gmail..com
Invalid Email 4         noptixqa@192.168.1.1.0
Invalid Email 5         noptixqa.@gmail.com
Invalid Email 6         noptixq..a@gmail.c
Invalid Email 7         noptixqa@-gmail.com
Invalid Email 8         ${SPACE}
Invalid Email 9         ${EMAIL UNREGISTERED}
Empty Email             ${EMPTY}

*** Keywords ***
Restart
    Close Browser
    Open Restore Password Dialog

Open Restore Password Dialog
    Run Keyword If    "${LANGUAGE}"=="he_IL"    Set Suite Variable    ${EMAIL INVALID}    //span[@ng-if='restorePassword.email.$touched && restorePassword.email.$error.email' and contains(text(),'${EMAIL INVALID TEXT}')]
    Run Keyword If    "${LANGUAGE}"=="he_IL"    Set Suite Variable    ${EMAIL IS REQUIRED}    //span[@ng-if='restorePassword.email.$touched && restorePassword.email.$error.required' and contains(text(),'${EMAIL IS REQUIRED TEXT}')]
    ${email}    Get Random Email    ${BASE EMAIL}
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    ${link}    Get Email Link    ${email}    activate
    Go To    ${link}[1]
    Go To    ${url}/restore_password
    Wait Until Elements Are Visible    ${RESTORE PASSWORD EMAIL INPUT}    ${RESET PASSWORD BUTTON}

Test Email Invalid
    [Arguments]   ${email}
    Input Text    ${RESTORE PASSWORD EMAIL INPUT}    ${email}
    Click Button    ${RESET PASSWORD BUTTON}
    Run Keyword Unless    '${email}'=='${EMAIL UNREGISTERED}'    Check Email Outline    ${email}
    Run Keyword If    '${email}'=='${EMAIL UNREGISTERED}'    Check For Alert Dismissable    ${CANNOT SEND CONFIRMATION EMAIL} ${ACCOUNT DOES NOT EXIST}

Check Email Outline
    [Arguments]    ${email}
    Wait Until Element Is Visible    ${RESTORE PASSWORD EMAIL INPUT}/parent::div/parent::div[contains(@class,'has-error')]
    Run Keyword If    "${email}"=="${EMPTY}" or "${email}"=="${SPACE}"    Element Should Be Visible    ${EMAIL IS REQUIRED}
    Run Keyword Unless    "${email}"=="${EMPTY}" or "${email}"=="${SPACE}"    Element Should Be Visible    ${EMAIL INVALID}