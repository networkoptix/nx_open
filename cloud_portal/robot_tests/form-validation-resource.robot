*** Settings ***
Resource          resource.robot

*** Keywords ***
Form Validation
    [arguments]    ${form name}    ${first name}=mark    ${last name}=hamill    ${email}=${EMAIL OWNER}    ${password}=${BASE PASSWORD}    ${password2}=${BASE PASSWORD}
    Run Keyword If    "${form name}"=="Log In"    Log In Form Validation   ${email}    ${password}
    Run Keyword If    "${form name}"=="Register"    Register Form Validation    ${first name}    ${last name}    ${email}    ${password}
    Run Keyword If    "${form name}"=="Change Password"    Change Password Form Validation    ${password}    ${password2}

Log In Form Validation
    [Arguments]    ${email}    ${password}
    Input Text    ${EMAIL INPUT}    ${email}
    Input Text    ${PASSWORD INPUT}    ${password}
    click button    ${LOG IN BUTTON}

Register Form Validation
    [arguments]    ${first name}    ${last name}    ${email}    ${password}
    Input Text    ${REGISTER FIRST NAME INPUT}    ${first name}
    Input Text    ${REGISTER LAST NAME INPUT}    ${last name}
    Input Text    ${REGISTER EMAIL INPUT}    ${email}
    Input Text    ${REGISTER PASSWORD INPUT}    ${password}
    click button    ${CREATE ACCOUNT BUTTON}

Change Password Form Validation
    [arguments]    ${old password}    ${new password}
    Input Text    ${CURRENT PASSWORD INPUT}    ${old password}
    Input Text    ${NEW PASSWORD INPUT}    ${new password}
    Click Button    ${CHANGE PASSWORD BUTTON}