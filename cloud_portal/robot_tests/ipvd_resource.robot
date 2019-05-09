*** Settings ***
Resource          resource.robot

*** Variables ***
${IPVD TABLE ROWS}    ${IPVD TABLE}//tr

*** Keywords ***
Go To IPVD page
    Go To    ${url}/ipvd
    Validate on IPVD page

Open IPVD page and Log In
    Open Browser and go to URL    ${url}/ipvd
    Validate on IPVD page
    Log In    ${EMAIL OWNER}    ${BASE PASSWORD}
    Validate Log In
    Wait Until Element Is Not Visible    ${LOG IN MODAL}
    Go To IPVD page  #Have to call it a 2nd time to get back onto the IPVD page after logging in

IPVD Text Search
    [Arguments]    ${SearchString}
    Click Element    ${IPVD SEARCH BAR}
    Element Should Be Focused    ${IPVD SEARCH BAR}
    Input Text    ${IPVD SEARCH BAR}    ${SearchString}
    Wait Until Element Is Visible    ${IPVD TABLE FIRST ITEM}

IPVD Table Row Count
    [Arguments]    ${AllPages}=False
    Wait Until Element Is Visible    ${IPVD TABLE}
    #TODO: Implement call to paginator if ${AllPages}=True
    ${rowCount}=   Get Element Count    ${IPVD TABLE}//tr
    [return]    ${rowCount}

IPVD Select Device From Table By Value
    [Arguments]    ${SearchString}
    Wait Until Element Is Visible    ${IPVD TABLE}
    Element Text Should Contain    ${IPVD TABLE}    ${SearchString}
    ${rowLocator}=   ${IPVD TABLE}
    : FOR ${rowIndex} IN RANGE 1 ${rowCount}+1
    \ ${curText} Get Text ${rowLocator}[${rowIndex}]/td[${column}]/a
    \ Exit For Loop If '${curText}' == '${cellText}'
    \ ${rowNumber} Set Variable ${rowIndex}
    Click Element ${rowLocator}[${rowIndex}]/td[${column}]/a
    [Return]    ${rowNumber}

IPVD Select Device From Table By Row Number
    [Arguments]    ${RowNumber}=1
    Wait Until Element Is Visible    ${IPVD TABLE}
    ${rows}=   Get WebElements    ${IPVD TABLE ROWS}
    ${rowCount}=   Get Element Count    ${IPVD TABLE ROWS}
    Run Keyword If    ${rowCount}-1>=${RowNumber}    Click Element    ${rows}[${RowNumber}]

IPVD Select Device From Table Randomly
    Wait Until Element Is Visible    ${IPVD TABLE}
    ${rows}=   Get WebElements    ${IPVD TABLE ROWS}
    ${rowCount}=   Get Element Count    ${IPVD TABLE ROWS}
    ${RowNumber}=   Evaluate    random.randint(1,${rowCount}-1)    modules=random
    Run Keyword If    ${rowCount}-1>=${RowNumber}    Click Element    ${rows}[${RowNumber}]

Advaced search filters text
    [Arguments]    ${filters}
    Return From Keyword    //ipvd/span[contains(text(),"${filters}"]

Validate on IPVD page
    Wait Until Elements Are Visible
    ...    ${IPVD TITLE}
    ...    ${IPVD SEARCH BAR}
    ...    ${IPVD ADVANCED SEARCH BUTTON}
    ...    ${IPVD MANFUACTURERS PANE}
    ...    ${IPVD DEVICES PANE}

Open New Browser On Failure
    Close Browser
    #Open Browser and go to URL    ${url}/ipvd
    Open Browser
    Go To IPVD page

Restart
    Register Keyword To Run On Failure    NONE
    ${status}=   Run Keyword And Return Status    Validate Log In
    Register Keyword To Run On Failure    Failure Tasks
    Run Keyword If    ${status}    Log Out
    Go To    ${url}/ipvd

Validate Request Form Initial State
    Wait Until Element Is Visible    ${IPVD FEEDBACK}
    ${result}=   Get Checkbox Value    ${IPVD FEEDBACK CONTACT ME}
    Should Be True    ${result}
    ${result}=   Get Checkbox Value    ${IPVD FEEDBACK AGREE}
    Should Not Be True    ${result}

Validate Privacy Policy
    Element Should Be Visible    ${IPVD FEEDBACK PRIVACY POLICY}
    ${url}=   Get Element Attribute    ${IPVD FEEDBACK PRIVACY POLICY}    href
    Should Contain    ${url}    /content/privacy    #TODO: CLOUD-2949
    #Should Contain    ${url}    ${PRIVACY POLICY URL}
    Click Element    ${IPVD FEEDBACK PRIVACY POLICY}
    @{windows}=   Get Window Handles
    ${numWindows}=   Get Length    ${windows}
    Should Be True    ${numWindows} == 2
    Select Window    @{windows}[1]
    Location Should Be    ${url}    #TODO: CLOUD-2949
    #Location Should Be    ${PRIVACY POLICY URL FULL}
    Wait Until Element Is Visible    ${PRIVACY POLICY HEADER}
    Close Window
    Select Window    @{windows}[0]

Submit Feedback/Request Form
    [Arguments]    ${Your Name}    ${Email}    ${Message}    ${Contact Me}    ${Agree to Privacy Policy}
    Input Text    ${IPVD FEEDBACK YOUR NAME}    ${Your Name}
    Sleep    0.25
    Input Text    ${IPVD FEEDBACK EMAIL}    ${Email}
    Sleep    0.25
    Input Text    ${IPVD FEEDBACK MESSAGE}    ${Message}
    Sleep    0.25
    Set Checkbox Value    ${IPVD FEEDBACK CONTACT ME}    ${Contact Me}
    Set Checkbox Value    ${IPVD FEEDBACK AGREE}    ${Agree to Privacy Policy}
    Sleep    0.25
    Click Button    ${IPVD FEEDBACK SEND BUTTON}
    Sleep    2

Validate Message Sent
    Page Should Not Contain Element    ${IPVD FEEDBACK}
    Check For Alert    Message has been sent.
    #TODO: Check email and verify submitted data received

Validate Message Not Sent
    Page Should Contain Element    ${IPVD FEEDBACK}
    Validate Input Field State    ${IPVD FEEDBACK EMAIL}/../..    False
