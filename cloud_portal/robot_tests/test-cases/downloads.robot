*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Setup        Restart
#Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers

*** Variables ***
${email}           ${EMAIL OWNER}
${password}        ${BASE PASSWORD}
${url}             ${ENV}

*** Keywords ***
Restart
    Register Keyword To Run On Failure    NONE

    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}
    Validate Log Out

Open New Browser On Failure
    Close Browser
    Open Browser and go to URL    ${url}

*** Test Cases ***
# Downloads page
Download link is in the footer
    Wait Until Element Is Visible    ${DOWNLOAD LINK}

Download link takes you to the /downloads page
    Wait Until Element Is Visible    ${DOWNLOAD LINK}
    Click Link    ${DOWNLOAD LINK}
    Location Should Be    ${url}/download
    Wait Until Element Is Visible    ${LOG IN MODAL}

Going to the downloads page anonymous asks for login and closing takes you back to home
    Wait Until Element Is Visible    ${DOWNLOAD LINK}
    Click Link    ${DOWNLOAD LINK}
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Click Button    ${LOG IN CLOSE BUTTON}
    Location Should Be    ${url}/

Going to the downloads page anonymous asks for login and login shows downloads page
    Wait Until Element Is Visible    ${DOWNLOAD LINK}
    Click Link    ${DOWNLOAD LINK}
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Log In    ${email}    ${password}    button=None
    Validate Log In
#    Wait Until Element Is Visible    locator    timeout    error

#Make sure each tab changes the text to show the corresponding OS and url
# Verify that download links start downloads

# Downloads histroy page
#History link is in the account dropdown
#History link takes you to the /downloads/history page
#Going to the history page anonymous asks for login and closing takes you back to home
#Going to the history page anonymous asks for login and login shows downloads page
#Going to the history page ananymous and logging in with someone who doesn't have access takes you to 404
#Going to the history page while logged in as someone who doesn't have access takes you to 404
#Make sure each tab changes to a unique release number
#Make sure expandable sections show options
# Verify that download links start downloads