*** Settings ***
Resource          resource.robot
Test Setup        Restart
Test Teardown     Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Setup       Open Browser and go to URL    ${url}
Suite Teardown    Close All Browsers
Force Tags        Threaded File

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${ENV}

*** Keywords ***
Advaced search filters text
    [Arguments]    ${fiters}
    Return From Keyword    //ipvd/span[contains(text(),"${filters}"]

Validate on ipvd page
    Wait Until Elements Are Visible    ${IPVD TITLE}    ${IPVD SEARCH BAR}    ${IPVD ADVANCED SEARCH BUTTON}
    ...                                ${IPVD MANFUACTURERS PANE}    ${IPVD DEVICES PANE}

Open New Browser On Failure
    Close Browser
    Open Browser and go to URL    ${url}/ipvd

Restart
    Register Keyword To Run On Failure    NONE
    ${status}    Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}

*** Test Cases ***
IPVD page loads without login
    Go To    ${url}/ipvd
    #need to add in the disclaimer at the bottom
    Validate on ipvd page

IPVD page loads while logged in
    Log In    ${EMAIL OWNER}    ${password}
    Validate Log In
    Go To    ${url}/ipvd
    Validate on ipvd page

#Submit request can be closed by 'X', cancel, and escape
#Submit request cannot be close by clicking outside the form
#Submit request correctly sends request
Text search correctly finds manufacturers
    Go To    ${url}/ipvd
    Validate on ipvd page
    Click Element    ${IPVD SEARCH BAR}
    Element Should Be Focused    ${IPVD SEARCH BAR}
    Input Text    ${IPVD SEARCH BAR}    hanwha
    Wait Until Element Is Visible    ${IPVD FIRST TABLE ITEM}
    Element Text Should Be    ${IPVD FIRST TABLE ITEM}/td[1]/div     Hanwha Techwin (Samsung)


#Text search correctly finds models
#Text search correctly finds resolutions
#Selecting manufacturer from landing page shows cameras for that manufacturer
#Selecting device from landing page shows cameras with the appropriate feature
#Selecting a device from the landing page with two filters works correctly
#Advanced search Minimum Resoltion dropdown applies filter correctly
#Advanced search Manufacturers dropdown applies filter(s) correctly
#Advanced search Types dropdown applies filter(s) correctly
#Advanced search Feature selection applies filter correctly
#Advanced search 2-way Audio and Audio filters interact correctly
#Advanced search PTZ and Advanced PTZ filters interact correctly
#Advanced search Minimum Res and Manufacturers interact correctly
#Advanced search Manufacturers and Types interact correctly
#Advanced search Types and Features interact correctly
#Clear search button clears all filters
#Column sorting for each column works as expected
#Data in table matches data in camera details
#Clicking the 'X' closes camera details
#Search in google works
#Page can be changed by next, previous, and clicking on visible numbers
#Export all to CSV works