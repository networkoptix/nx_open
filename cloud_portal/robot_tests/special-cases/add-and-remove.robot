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
    Open Browser and go to URL    ${url}
    Log in to Auto Tests System    ${email}
    Wait Until Element Is Visible    ${SHARE BUTTON SYSTEMS}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email1}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email1}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email2}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email2}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email3}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email3}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email4}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email4}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email5}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email5}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email6}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email6}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email7}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email7}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email8}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email8}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email9}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email9}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email10}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email10}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email11}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email11}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email12}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email12}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email13}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email13}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email14}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email14}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email15}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email15}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email16}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email16}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email17}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email17}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email18}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email18}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email19}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email19}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email20}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email20}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email21}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email21}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email22}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email22}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email23}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email23}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email24}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email24}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email25}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email25}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email26}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email26}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email27}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email27}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email28}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email28}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email29}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email29}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Click Button    ${SHARE BUTTON SYSTEMS}
    ${random email30}    Get Random Email
    Wait Until Elements Are Visible    ${SHARE EMAIL}    ${SHARE BUTTON MODAL}
    Input Text    ${SHARE EMAIL}    ${random email30}
    Click Button    ${SHARE BUTTON MODAL}
    Check For Alert2    ${NEW PERMISSIONS SAVED}
    Register Keyword To Run On Failure    Capture Page Screenshot
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email1}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email2}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email3}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email4}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email5}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email6}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email7}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email8}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email9}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email10}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email11}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email12}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email13}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email14}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email15}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email16}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email17}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email18}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email19}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email20}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email21}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email22}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email23}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email24}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email25}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email26}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email27}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email28}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email29}
    Run Keyword And Continue On Failure    Remove User Permissions    ${random email30}
    Close Browser
