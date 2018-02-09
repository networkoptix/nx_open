*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${email}           ${EMAIL OWNER}
${password}        ${BASE PASSWORD}
${url}             ${CLOUD TEST}
${share dialogue}

*** Keywords ***
Log in to Auto Tests System
    [arguments]    ${email}
    Go To    ${url}/systems/${AUTO TESTS SYSTEM ID}
    Log In    ${email}    ${password}    None
    Run Keyword If    '${email}' == '${EMAIL OWNER}'    Wait Until Elements Are Visible    ${DISCONNECT FROM NX}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword If    '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}    ${RENAME SYSTEM}
    Run Keyword Unless    '${email}' == '${EMAIL OWNER}' or '${email}' == '${EMAIL ADMIN}'    Wait Until Elements Are Visible    ${DISCONNECT FROM MY ACCOUNT}    ${OPEN IN NX BUTTON}

*** Test Cases ***
Share button - opens dialog
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    Wait Until Elements Are Visible    ${SHARE BUTTON SYSTEMS}    ${OPEN IN NX BUTTON}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Element Is Visible    ${SHARE MODAL}
    Close Browser

Sharing link /systems/{system_id}/share - opens dialog
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    ${location}    Get Location
    Go To    ${location}/share
    Wait Until Element Is Visible    ${SHARE MODAL}
    Close Browser

Sharing link for anonymous - first ask login, then show share dialog
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    ${location}    Get Location
    Log Out
    Go To    ${location}/share
    Log In    ${email}    ${password}    button=None
    Wait Until Element Is Visible    ${SHARE MODAL}
    Close Browser

After closing dialog, called by link - clear link
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    ${location}    Get Location

#Check Cancel Button
    Go To    ${location}/share
    Wait Until Elements Are Visible    ${SHARE MODAL}    ${SHARE CANCEL}
    Click Button    ${SHARE CANCEL}
    Wait Until Element Is Not Visible    ${SHARE MODAL}
    Location Should Be    ${location}

#Check 'X' Button
    Go To    ${location}/share
    Wait Until Elements Are Visible    ${SHARE MODAL}    ${SHARE CLOSE}
    Wait Until Element Is Visible    ${SHARE CLOSE}
    Click Button    ${SHARE CLOSE}
    Wait Until Element Is Not Visible    ${SHARE MODAL}
    Location Should Be    ${location}

#Check Background Click
    Go To    ${location}/share
    Wait Until Elements Are Visible    ${SHARE MODAL}    //div[@uib-modal-window="modal-window"]
    Click Element    //div[@uib-modal-window="modal-window"]
    Wait Until Element Is Not Visible    ${SHARE MODAL}
    Location Should Be    ${location}
    Close Browser

Sharing roles are ordered: more access is on top of the list with options
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Element Text Should Be    ${SHARE PERMISSIONS DROPDOWN}    Administrator\nAdvanced Viewer\nViewer\nLive Viewer\nCustom
    Close Browser

When user selects role - special hint appears
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    Click Button    ${SHARE BUTTON SYSTEMS}
#Adminstrator check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS ADMINISTRATOR}
    Click Element    ${SHARE PERMISSIONS ADMINISTRATOR}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    Unrestricted access including the ability to share
#Advanced Viewer Check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS ADVANCED VIEWER}
    Click Element    ${SHARE PERMISSIONS ADVANCED VIEWER}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    Can view live video, browse the archive, configure cameras, control PTZ etc
#Viewer Check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS VIEWER}
    Click Element    ${SHARE PERMISSIONS VIEWER}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    Can view live video and browse the archive
#Live Viewer Check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS LIVE VIEWER}
    Click Element    ${SHARE PERMISSIONS LIVE VIEWER}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    Can only view live video
#Custom Check
    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
    Click Element    ${SHARE PERMISSIONS DROPDOWN}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS CUSTOM}
    Click Element    ${SHARE PERMISSIONS CUSTOM}
    Wait Until Element Is Visible    ${SHARE PERMISSIONS HINT}
    Element Text Should Be    ${SHARE PERMISSIONS HINT}    Use the Nx Cloud Client application to set up custom permissions
    Close Browser

Sharing works
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    New permissions saved
    Check User Permissions    ${random email}    Custom
    Remove User Permissions    ${random email}
    Close Browser

Edit permission works
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    New permissions saved
    Edit User Permissions In Systems    ${random email}    Viewer
    Check User Permissions    ${random email}    Viewer
    Edit User Permissions In Systems    ${random email}    Custom
    Check User Permissions    ${random email}    Custom
    Remove User Permissions    ${random email}
    Close Browser

Share with registered user - sends him notification
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    Verify In System    Auto Tests
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${EMAIL NOPERM}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert    New permissions saved
    Check User Permissions    ${EMAIL NOPERM}    Custom
    Open Mailbox    host=imap.gmail.com    password=qweasd!@#    port=993    user=noptixqa@gmail.com    is_secure=True
    ${email}    Wait For Email    recipient=${EMAIL NOPERM}    subject=TestFirstName TestLastName invites you to Nx Cloud    timeout=120
    Remove User Permissions    ${EMAIL NOPERM}
    Close Browser