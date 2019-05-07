*** Settings ***
Resource          resource.robot
Suite Setup       Open Browser and go to URL    ${url}
Test Setup        Restart
Test Teardown     Close Browser    #Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
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

Validate Request Form Initial State
    Wait Until Element Is Visible    ${IPVD FEEDBACK}

    ${result}    Get Checkbox Value    ${IPVD FEEDBACK CONTACT ME}
    Should Be True    ${result}

    ${result}    Get Checkbox Value    ${IPVD FEEDBACK AGREE}
    Should Not Be True    ${result}

Validate Privacy Policy
    @{windowsBefore}    Get Window Handles
    Element Should Be Visible    ${IPVD FEEDBACK PRIVACY POLICY}
    ${url}    Get Element Attribute    ${IPVD FEEDBACK PRIVACY POLICY}    href
    Should Contain    ${url}    /content/privacy
    Click Element    ${IPVD FEEDBACK PRIVACY POLICY}
    @{windowsAfter}    Get Window Handles
    ${numWindowsBefore}    Get Length    ${windowsBefore}
    ${numWindowsAfter}    Get Length    ${windowsAfter}
    Should Be True    ${numWindowsAfter} > 1
    Should Be True    ${numWindowsAfter} == ${numWindowsBefore}+1
    ${indexLast}=   Evaluate    ${numWindowsAfter}-1
    Select Window    @{windowsAfter}[${indexLast}]
    Location Should Be    ${url}
    Wait Until Element Is Visible    ${PRIVACY POLICY HEADER}
    Close Window
    Select Window    @{windowsAfter}[0]

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

IPVD Request Form Basic Validations
    [tags]    C48969
    Go To    ${url}/ipvd
    Wait Until Element Is Visible    ${SUBMIT A REQUEST}
    Click Element    ${SUBMIT A REQUEST}
    Wait Until Element Is Visible    ${IPVD FEEDBACK}
    Validate Request Form Initial State
    Validate Privacy Policy
    Click Button    ${IPVD FEEDBACK SEND BUTTON}
    #Name, email, message, and agreeing to privacy policy fields turn red
    Validate Input Field State    ${IPVD FEEDBACK YOUR NAME}/../..    False
    Validate Input Field State    ${IPVD FEEDBACK EMAIL}/../..    False
    Validate Input Field State    ${IPVD FEEDBACK MESSAGE}/../..    False
    Validate Input Field State    ${IPVD FEEDBACK AGREE}/../..    False
    Click Button    ${IPVD FEEDBACK CANCEL BUTTON}
    #TODO: Verify Table of devices and camera info panel did not change
    Click Element    ${SUBMIT A REQUEST}
    Wait Until Element Is Visible    ${IPVD FEEDBACK}
    Click Button    ${IPVD FEEDBACK CLOSE BUTTON}
    #TODO: Verify Table of devices and camera info panel did not change