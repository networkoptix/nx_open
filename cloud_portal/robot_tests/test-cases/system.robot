*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${CLOUD TEST}

*** Keywords ***
Log in to Auto Tests System
    [arguments]    ${email}
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Log In    ${email}    ${password}    None
    Run Keyword If    '${email}' == '${EMAIL OWNER}'    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword If    '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword Unless    '${email}' == '${EMAIL OWNER}' or '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${OPEN IN NX BUTTON}

*** Test Cases ***
has system name, owner and OpenInNx button visible on systems page
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTO TESTS TITLE}    ${AUTO TESTS USER}    ${AUTO TESTS OPEN NX}
    Element Text Should Be    ${AUTO TESTS TITLE}    Auto Tests
    Close Browser

shows offline status and does not show open in nx button when offline
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Wait Until Elements Are Visible    ${SYSTEMS SEARCH INPUT}    ${AUTOTESTS OFFLINE}
    Wait Until Element Is Not Visible    ${AUTOTESTS OFFLINE OPEN NX}
    Close Browser

should confirm, if owner deletes system (You are going to disconnect your system from cloud)
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${EMAIL OWNER}
    Click Button    ${DISCONNECT FROM NX}
    Wait Until Elements Are Visible    ${DISCONNECT FORM}    ${DISCONNECT FORM HEADER}
    Close Browser

should confirm, if not owner deletes system (You will loose access to this system)
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL NOT OWNER}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${DISCONNECT FROM MY ACCOUNT}
    Click Button    ${DISCONNECT FROM MY ACCOUNT}
    Wait Until Element Is Visible    ${DISCONNECT MODAL WARNING}
    Close Browser

Cancel should cancel disconnection and disconnect should remove it when not owner
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL NOT OWNER}    ${password}
    Validate Log In
    Wait Until Element Is Visible    ${DISCONNECT FROM MY ACCOUNT}
    Click Button    ${DISCONNECT FROM MY ACCOUNT}
    Wait Until Elements Are Visible    ${DISCONNECT MODAL WARNING}    ${DISCONNECT MODAL CANCEL}
    Click Button    ${DISCONNECT MODAL CANCEL}
    Wait Until Element Is Not Visible    ${DISCONNECT MODAL WARNING}
    Wait Until Page Does Not Contain Element    //div[@modal-render='true']
    Wait Until Element Is Visible    ${DISCONNECT FROM MY ACCOUNT}
    Click Button    ${DISCONNECT FROM MY ACCOUNT}
    Wait Until Elements Are Visible    ${DISCONNECT MODAL WARNING}    ${DISCONNECT MODAL DISCONNECT BUTTON}
    Click Button    ${DISCONNECT MODAL DISCONNECT BUTTON}
    Wait Until Element Is Visible    ${YOU HAVE NO SYSTEMS}
    Log Out
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${EMAIL NOT OWNER}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Close Browser

has Share button, visible for admin and owner
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${EMAIL OWNER}
    Log Out
    Log in to Auto Tests System    ${EMAIL ADMIN}
    Close Browser

does not show Share button to viewer, advanced viewer, live viewer
#This allows the expected error to not run the close browser action
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${EMAIL VIEWER}
    Register Keyword To Run On Failure    NONE
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Register Keyword To Run On Failure    Failure Tasks
    Element Should Not Be Visible    ${SHARE BUTTON SYSTEMS}
    Log Out
    Log in to Auto Tests System    ${EMAIL ADV VIEWER}
    Register Keyword To Run On Failure    NONE
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Register Keyword To Run On Failure    Failure Tasks
    Element Should Not Be Visible    ${SHARE BUTTON SYSTEMS}
    Log Out
    Log in to Auto Tests System    ${EMAIL LIVE VIEWER}
    Register Keyword To Run On Failure    NONE
    Run Keyword And Expect Error    *    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Register Keyword To Run On Failure    Failure Tasks
    Element Should Not Be Visible    ${SHARE BUTTON SYSTEMS}
    Close Browser

should open System page by link to not authorized user and redirect to homepage, if he does not log in
    Open Browser and go to URL    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Click Button    ${LOG IN CLOSE BUTTON}
    Wait Until Element Is Visible    ${JUMBOTRON}
    Close Browser

should open System page by link to not authorized user and show it, after owner logs in
    Open Browser and go to URL    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Log In    ${EMAIL OWNER}   ${password}    None
    Verify In System    Auto Tests
    Close Browser

should open System page by link to user without permission and show alert (System info is unavailable: You have no access to this system)
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL NOPERM}    ${password}
    Validate Log In
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Wait Until Element Is Visible    ${SYSTEM NO ACCESS}
    Close Browser

should open System page by link not authorized user, and show alert if logs in and has no permission
    Open Browser and go to URL    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Log In    ${EMAIL NOPERM}   ${password}    None
    Wait Until Element Is Visible    ${SYSTEM NO ACCESS}
    Close Browser

should display same user data as user provided during registration (stress to cyrillic)
#create user
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    ${CYRILLIC NAME}    ${CYRILLIC NAME}    ${email}    ${password}
    Activate    ${email}
#share system with new user
    Log in to Auto Tests System    ${EMAIL OWNER}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE PERMISSIONS DROPDOWN}
    Input Text    ${SHARE EMAIL}    ${email}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS ADMINISTRATOR}
    Click Element    ${SHARE PERMISSIONS ADMINISTRATOR}
    Wait Until Element Is Visible    ${SHARE BUTTON MODAL}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Log Out

#verify user was added with appropriate name
    Log In    ${email}    ${password}
    Wait Until Element Is Visible    //td[contains(text(),'${CYRILLIC NAME} ${CYRILLIC NAME}')]

#remove new user from system
    Log Out
    Log in to Auto Tests System    ${EMAIL OWNER}
    Remove User Permissions    ${email}
    Close Browser

should display same user data as showed in user account (stress to cyrillic)
#create user
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Activate    ${email}
#share system with new user
    Log in to Auto Tests System    ${EMAIL OWNER}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE PERMISSIONS DROPDOWN}
    Input Text    ${SHARE EMAIL}    ${email}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS ADMINISTRATOR}
    Click Element    ${SHARE PERMISSIONS ADMINISTRATOR}
    Wait Until Element Is Visible    ${SHARE BUTTON MODAL}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    ${NEW PERMISSIONS SAVED}
    Log Out

    Go To    ${url}/account
    Log In    ${email}    ${password}    None
    Validate Log In
    Wait Until Textfield Contains    ${ACCOUNT FIRST NAME}    mark
    Wait Until Textfield Contains    ${ACCOUNT LAST NAME}    hamill
    Clear Element Text    ${ACCOUNT FIRST NAME}
    Input Text    ${ACCOUNT FIRST NAME}    ${CYRILLIC NAME}
    Clear Element Text    ${ACCOUNT LAST NAME}
    Input Text    ${ACCOUNT LAST NAME}    ${CYRILLIC NAME}
    sleep    .15
    Wait Until Element Is Visible    ${ACCOUNT SAVE}
    Click Button    ${ACCOUNT SAVE}
    Check For Alert    ${YOUR ACCOUNT IS SUCCESSFULLY SAVED}
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Wait Until Element Is Visible    //td[contains(text(),'${CYRILLIC NAME} ${CYRILLIC NAME}')]

    #remove new user from system
    Log Out
    Log in to Auto Tests System    ${EMAIL OWNER}
    Remove User Permissions    ${email}
    Close Browser