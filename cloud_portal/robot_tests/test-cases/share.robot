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
    Open Browser and go to URL    ${url}
    Log In    ${email}    ${password}
    Select Auto Tests System

*** Test Cases ***
#Share button - opens dialog
#    Log in to Auto Tests System
#    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
#    Click Button    ${SHARE BUTTON SYSTEMS}
#    Wait Until Element Is Visible    ${SHARE MODAL}
#    Element Should Be Visible    ${SHARE MODAL}
#    Close Browser
#
#Sharing link /systems/{system_id}/share - opens dialog
#    Log in to Auto Tests System
#    ${location}    Get Location
#    Go To    ${location}/share
#    Wait Until Element Is Visible    ${SHARE MODAL}
#    Element Should Be Visible    ${SHARE MODAL}
#    Close Browser
#
#Sharing link for anonymous - first ask login, then show share dialog
#    Log in to Auto Tests System
#    ${location}    Get Location
#    Log Out
#    Validate Log Out
#    Go To    ${location}/share
#    Log In    ${email}    ${password}    button=None
#    Wait Until Element Is Visible    ${SHARE MODAL}
#    Element Should Be Visible    ${SHARE MODAL}
#    Close Browser
#
#After closing dialog, called by link - clear link
#    Log in to Auto Tests System
#    ${location}    Get Location
#
##Check Cancel Button
#    Go To    ${location}/share
#    Wait Until Element Is Visible    ${SHARE MODAL}
#    Element Should Be Visible    ${SHARE MODAL}
#    Wait Until Element Is Visible    ${SHARE CANCEL}
#    Click Button    ${SHARE CANCEL}
#    Wait Until Element Is Not Visible    ${SHARE MODAL}
#    Location Should Be    ${location}
#
##Check 'X' Button
#    Go To    ${location}/share
#    Wait Until Element Is Visible    ${SHARE MODAL}
#    Element Should Be Visible    ${SHARE MODAL}
#    Wait Until Element Is Visible    ${SHARE CLOSE}
#    Click Button    ${SHARE CLOSE}
#    Wait Until Element Is Not Visible    ${SHARE MODAL}
#    Location Should Be    ${location}
#
##Check Background Click
#    Go To    ${location}/share
#    Wait Until Element Is Visible    ${SHARE MODAL}
#    Element Should Be Visible    ${SHARE MODAL}
#    Wait Until Element Is Visible    //div[@uib-modal-window="modal-window"]
#    Click Element    //div[@uib-modal-window="modal-window"]
#    Wait Until Element Is Not Visible    ${SHARE MODAL}
#    Location Should Be    ${location}
#    Close Browser
#
#Sharing roles are ordered: more access is on top of the list with options
#    Log in to Auto Tests System
#    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
#    Click Button    ${SHARE BUTTON SYSTEMS}
#    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
#    Click Element    ${SHARE PERMISSIONS DROPDOWN}
#    Element Text Should Be    ${SHARE PERMISSIONS DROPDOWN}    Administrator\nAdvanced Viewer\nViewer\nLive Viewer\nCustom
#    Close Browser
#
#When user selects role - special hint appears
#    Log in to Auto Tests System
#    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
#    Click Button    ${SHARE BUTTON SYSTEMS}
##Adminstrator check
#    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
#    Click Element    ${SHARE PERMISSIONS DROPDOWN}
#    Wait Until Element Is Visible    ${PERMISSIONS ADMINISTRATOR}
#    Click Element    ${PERMISSIONS ADMINISTRATOR}
#    Wait Until Element Is Visible    ${PERMISSIONS HINT}
#    Element Text Should Be    ${PERMISSIONS HINT}    Unrestricted access including the ability to share
##Advanced Viewer Check
#    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
#    Click Element    ${SHARE PERMISSIONS DROPDOWN}
#    Wait Until Element Is Visible    ${PERMISSIONS ADVANCED VIEWER}
#    Click Element    ${PERMISSIONS ADVANCED VIEWER}
#    Wait Until Element Is Visible    ${PERMISSIONS HINT}
#    Element Text Should Be    ${PERMISSIONS HINT}    Can view live video, browse the archive, configure cameras, control PTZ etc
##Viewer Check
#    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
#    Click Element    ${SHARE PERMISSIONS DROPDOWN}
#    Wait Until Element Is Visible    ${PERMISSIONS VIEWER}
#    Click Element    ${PERMISSIONS VIEWER}
#    Wait Until Element Is Visible    ${PERMISSIONS HINT}
#    Element Text Should Be    ${PERMISSIONS HINT}    Can view live video and browse the archive
##Live Viewer Check
#    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
#    Click Element    ${SHARE PERMISSIONS DROPDOWN}
#    Wait Until Element Is Visible    ${PERMISSIONS LIVE VIEWER}
#    Click Element    ${PERMISSIONS LIVE VIEWER}
#    Wait Until Element Is Visible    ${PERMISSIONS HINT}
#    Element Text Should Be    ${PERMISSIONS HINT}    Can only view live video
##Custom Check
#    Wait Until Element Is Visible    ${SHARE PERMISSIONS DROPDOWN}
#    Click Element    ${SHARE PERMISSIONS DROPDOWN}
#    Wait Until Element Is Visible    ${PERMISSIONS CUSTOM}
#    Click Element    ${PERMISSIONS CUSTOM}
#    Wait Until Element Is Visible    ${PERMISSIONS HINT}
#    Element Text Should Be    ${PERMISSIONS HINT}    Use the Nx Cloud Client application to set up custom permissions

Edit permission works
    Log in to Auto Tests System
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email}    Get Random Email
    Wait Until Element Is Visible    ${SHARE EMAIL}
    Input Text    ${SHARE EMAIL}    ${random email}
    Wait Until Element Is Visible    ${SHARE BUTTON MODAL}
    Click Button    ${SHARE BUTTON MODAL}
    Edit User In System    ${random email}
    sleep    10