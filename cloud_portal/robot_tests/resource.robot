*** Settings ***

Library           Selenium2Library    screenshot_root_directory=\Screenshots    run_on_failure=Failure Tasks
Library           ImapLibrary
Library           NoptixLibrary
Resource          variables.robot

*** Keywords ***
Open Browser and go to URL
    [Arguments]    ${url}
    Open Browser    ${url}    Chrome
#    Maximize Browser Window
    Set Selenium Speed    0

Log In
    [arguments]    ${email}    ${password}    ${button}=${LOG IN NAV BAR}
    Run Keyword Unless    "${button}" == "None"    Wait Until Element Is Visible    ${button}
    Run Keyword Unless    "${button}" == "None"    Click Link    ${button}
    Wait Until Element Is Visible    ${EMAIL INPUT}
    input text    ${EMAIL INPUT}    ${email}

    Wait Until Element Is Visible    ${PASSWORD INPUT}
    input text    ${PASSWORD INPUT}    ${password}

    Wait Until Element Is Visible    ${LOG IN BUTTON}
    click button    ${LOG IN BUTTON}

Validate Log In
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Element Should Be Visible    ${ACCOUNT DROPDOWN}

Log Out
    Wait Until Element Is Visible    ${ACCOUNT DROPDOWN}
    Click Element    ${ACCOUNT DROPDOWN}
    Wait Until Element Is Visible    ${LOG OUT BUTTON}
    Click Link    ${LOG OUT BUTTON}

Validate Log Out
    Wait Until Element Is Visible    ${LOG IN NAV BAR}
    Element Should Be Visible    ${LOG IN NAV BAR}

Register
    [arguments]    ${first name}    ${last name}    ${email}    ${password}

    Wait Until Element Is Visible    ${FIRST NAME INPUT}
    Input Text    ${FIRST NAME INPUT}    ${first name}

    Wait Until Element Is Visible    ${LAST NAME INPUT}
    Input Text    ${LAST NAME INPUT}    ${last name}

    Wait Until Element Is Visible    ${REGISTER EMAIL INPUT}
    Input Text    ${REGISTER EMAIL INPUT}    ${email}

    Wait Until Element Is Visible    ${REGISTER PASSWORD INPUT}
    Input Text    ${REGISTER PASSWORD INPUT}    ${password}

    Wait Until Element Is Visible    ${CREATE ACCOUNT BUTTON}
    Click Button    ${CREATE ACCOUNT BUTTON}

Validate Register Email Received
    [arguments]    ${recipient}
    Open Mailbox    host=imap.gmail.com    password=qweasd!@#    port=993    user=noptixqa@gmail.com    is_secure=True
    ${email}    Wait For Email    recipient=${recipient}    timeout=120
    Should Not Be Equal    ${email}    ${EMPTY}
    Delete Email    ${email}

Get Activation Link
    [arguments]    ${recipient}
    Open Mailbox    host=imap.gmail.com    password=qweasd!@#    port=993    user=noptixqa@gmail.com    is_secure=True
    ${email}    Wait For Email    recipient=${recipient}    timeout=120
    ${links}    Get Links From Email    ${email}
    Go To    @{links}[1]

Select Auto Tests System
    Wait Until Element Is Visible    ${AUTO TESTS}
    Click Element    ${AUTO TESTS}

Edit User In System
    [arguments]    ${user email}
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}') and contains(@class,'user-email')]
    Mouse Over    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}') and contains(@class,'user-email')]
    Wait Until Element Is Visible    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/a[@ng-click='editShare(user)']
    Click Link    //tr[@ng-repeat='user in system.users']//td[contains(text(), '${user email}')]/following-sibling::td/a[@ng-click='editShare(user)']

Failure Tasks
    Capture Page Screenshot    selenium-screenshot-{index}.png
    Close Browser