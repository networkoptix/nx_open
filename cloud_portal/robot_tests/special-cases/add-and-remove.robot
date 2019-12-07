#This is a script which will add 30 users and then delete them.
#It was made for testing CLOUD-1675
*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${email}           ${EMAIL OWNER}
${password}        ${BASE PASSWORD}
${url}             ${CLOUD TEST}
${how many}        30

*** Keywords ***
Log in to Auto Tests System
    [arguments]    ${email}
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Log In    ${email}    ${password}    None
    Run Keyword If    '${email}' == '${EMAIL OWNER}'    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword If    '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword Unless    '${email}' == '${EMAIL OWNER}' or '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${OPEN IN NX BUTTON}
Check For Alert2
    [arguments]    ${alert text}
    Wait Until Element Is Visible    ${ALERT}
    Element Should Be Visible    ${ALERT}
    Element Text Should Be    ${ALERT}    ${alert text}
    Wait Until Page Does Not Contain Element    ${ALERT}

*** Test Cases ***
Add Then Remove
    @{emails}    Get Many Random Emails    ${how many}
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    :FOR    ${x}    IN RANGE    ${how many}
    \  Click Button    ${SHARE BUTTON SYSTEMS}
    \  Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    \  Input Text    ${SHARE EMAIL}    @{emails}[${x}]
    \  Click Button    ${SHARE BUTTON MODAL}
    \  Check For Alert2    ${NEW PERMISSIONS SAVED}
    Register Keyword To Run On Failure    Capture Page Screenshot
    :FOR    ${x}    IN RANGE    ${how many}
    \  Run Keyword And Continue On Failure    Remove User Permissions    @{emails}[${x}]
    Close Browser
