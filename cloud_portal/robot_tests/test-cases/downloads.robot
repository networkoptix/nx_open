*** Settings ***
Resource          ../resource.robot
Test Setup        Restart
#Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers
Force Tags        Threaded

*** Variables ***
${email}             ${EMAIL OWNER}
${password}          ${BASE PASSWORD}
${url}               ${ENV}
${other packages}    //div[contains(@class,"card-body")]//div[contains(@class, "installers")]//a

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

Go to download page
    Wait Until Element Is Visible    ${DOWNLOAD LINK}
    Click Link    ${DOWNLOAD LINK}
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Log In    ${email}    ${password}    button=None
    Validate Log In
    Wait Until Elements Are Visible    ${DOWNLOADS HEADER}    ${WINDOWS TAB}
    Click Link    ${WINDOWS TAB}

Check for file by OS
    [arguments]    ${os}
    Wait Until Element Is Visible    ${DOWNLOADS HEADER}
    Wait Until Element Is Visible    ${${os} TAB}
    Click Link    ${${os} TAB}
    Wait Until Element Is Visible    ${DOWNLOAD ${os} VMS LINK}
    ${url}    Get Element Attribute    ${DOWNLOAD ${os} VMS LINK}    href
    Check File Exists    ${url}

Check other packages
    ${packages}    Get WebElements    ${other packages}
    :FOR  ${element}  IN  @{packages}
    \  ${url}    Get Element Attribute    ${element}    href
    \  Check File Exists    ${url}

*** Test Cases ***
Download link is in the footer
    Wait Until Element Is Visible    ${DOWNLOAD LINK}

Download link takes you to the /downloads page
    Wait Until Element Is Visible    ${DOWNLOAD LINK}
    Click Link    ${DOWNLOAD LINK}
    Location Should Be    ${url}/download
#    Wait Until Element Is Visible    ${LOG IN MODAL}

Going to the downloads page anonymous asks for login and closing takes you back to home
    [tags]    C42069
    Wait Until Element Is Visible    ${DOWNLOAD LINK}
    Click Link    ${DOWNLOAD LINK}
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Click Button    ${LOG IN CLOSE BUTTON}
    Location Should Be    ${url}/

Going to the downloads page anonymous asks for login and login shows downloads page
    [tags]    C42069
    Go to download page

Going to the downloads page should show you the tab according to your OS
    [tags]    C41550
    Wait Until Element Is Visible    ${DOWNLOAD LINK}
    Click Link    ${DOWNLOAD LINK}
    Wait Until Element Is Visible    ${LOG IN CLOSE BUTTON}
    Log In    ${email}    ${password}    button=None
    Validate Log In
    Wait Until Elements Are Visible    ${DOWNLOADS HEADER}    ${WINDOWS TAB}
    ${os}    Get OS
    Wait Until Element Is Visible    //a[contains(@class,"active") and @id="${os}"]

Make sure each tab changes the text to show the corresponding OS and url
    Go to download page
    Wait Until Elements Are Visible    ${DOWNLOAD WINDOWS VMS LINK}    ${WINDOWS TAB}
    Click Link    ${WINDOWS TAB}
    Wait Until Element Is Visible    ${UBUNTU TAB}
    Click Link    ${UBUNTU TAB}
    Location Should Be    ${url}/download/Linux
    Wait Until Elements Are Visible    ${DOWNLOAD UBUNTU VMS LINK}    ${MAC OS TAB}
    Click Link    ${MAC OS TAB}
    Location Should Be    ${url}/download/MacOS
    Wait Until Elements Are Visible    ${DOWNLOAD MAC OS VMS LINK}    ${MAC OS TAB}

Validate the windows download links
    [tags]    C41552
    Go to download page
    Check for file by OS    WINDOWS
    Check other packages

Validate the ubuntu download links
    [tags]    C41552
    Go to download page
    Check for file by OS    UBUNTU
    Check other packages

Validate the mac download links
    [tags]    C41552
    Go to download page
    Check for file by OS    MAC OS
    Check other packages

Check Play Store Link
    [tags]    C41554
    Go to download page
    ${url}    Get Element Attribute    ${PLAY STORE DOWNLOAD BUTTON}    href
    Should Be Equal    ${url}    ${PLAY STORE LINK}
    Check File Exists    ${url}

Check iTunes Store Link
    [tags]    C41554
    Go to download page
    ${url}    Get Element Attribute    ${ITUNES STORE DOWNLOAD BUTTON}    href
    Should Be Equal    ${url}    ${ITUNES STORE LINK}
    Check File Exists    ${url}