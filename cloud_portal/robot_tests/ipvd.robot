*** Settings ***
Resource          ipvd_resource.robot
Suite Setup       Open Browser and go to URL    ${url}/ipvd
Test Setup        Restart
Test Teardown     Close Browser    #Run Keyword If Test Failed    Reset DB and Open New Browser On Failure
Suite Teardown    Close All Browsers
Force Tags        Threaded File

*** Variables ***
${url}         ${ENV}

*** Keywords ***


*** Test Cases ***
IPVD page loads without login
    Go To IPVD page

IPVD page loads while logged in
    Log In    ${EMAIL OWNER}    ${BASE PASSWORD}
    Validate Log In
    Go To IPVD page

#Submit request can be closed by 'X', cancel, and escape
#Submit request cannot be close by clicking outside the form
#Submit request correctly sends request

Text search correctly finds manufacturers
    Go To IPVD page
    IPVD Text Search    hanwha
    Element Text Should Be    ${IPVD TABLE FIRST ITEM}/td[1]/div     Hanwha Techwin (Samsung)


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
    Go To IPVD page
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
    Click Element    ${SUBMIT A REQUEST}
    Wait Until Element Is Visible    ${IPVD FEEDBACK}
    Click Button    ${IPVD FEEDBACK CLOSE BUTTON}

IPVD Feedback Form Basic Validations
    [tags]    C54182
    #IPVD page    Login=True
    #Wait Until Element Is Not Visible    ${LOG IN MODAL}
    Open IPVD page and Log In
    IPVD Text Search    Axis
    IPVD Select Device From Table Randomly
    Wait Until Element Is Visible    ${SEND DEVICE FEEDBACK}
    Click Element    ${SEND DEVICE FEEDBACK}
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