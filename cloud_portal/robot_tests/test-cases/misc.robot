*** Settings ***
Resource          ../resource.robot
Test Teardown     Close Browser
Force Tags        Threaded

*** Variables ***
${url}    ${ENV}

*** Test Cases ***
404 page shows when going to a url that doesn't exist and gives a link back to home page
    [tags]    C41565
    Open Browser and go to URL    ${url}/wfvyuieyuisgweyugv
    Wait Until Elements Are Visible    ${PAGE NOT FOUND}    ${TAKE ME HOME}
    Click Link    ${TAKE ME HOME}
    Location Should Be    ${url}/

Failed to access system page correctly shows when going to a non-existent system
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${BASE PASSWORD}
    Validate Log In
    Go To    ${url}/systems/htgfjtrdtrtyrrtydrydcrtydrtrdrtdrtdrtdrtd
    ${THIS LINK IS BROKEN TEXT}    Replace String    ${THIS LINK IS BROKEN TEXT}    <br>    ${EMPTY}
    ${THIS LINK IS BROKEN TEXT}    Replace String    ${THIS LINK IS BROKEN TEXT}    \n    ${EMPTY}
    :FOR    ${x}   IN RANGE    4
    \  ${THIS LINK IS BROKEN TEXT}    Replace String    ${THIS LINK IS BROKEN TEXT}    ${SPACE}${SPACE}    ${SPACE}
    Wait Until Elements Are Visible    //h1[text()="${SYSTEM NO ACCESS TEXT}"]    //h3[normalize-space()="${THIS LINK IS BROKEN TEXT}"]    //p//a[@href='/systems']/..

The logo takes you to the home page when not logged in
    [tags]    C41539
    Open Browser and go to URL    ${url}/register
    Wait Until Element Is Visible    ${LOGO LINK}
    Click Link    ${LOGO LINK}
    Location Should Be    ${url}/

The logo takes you to the systems page when not logged in
    [tags]    C41540
    Open Browser and go to URL    ${url}/register
    Log In    ${EMAIL OWNER}    ${BASE PASSWORD}
    Validate Log In
    Go To    ${url}/${AUTO_TESTS SYSTEM ID}
    Wait Until Element Is Visible    ${LOGO LINK}
    Click Link    ${LOGO LINK}
    Wait Until Element Is Visible    ${AUTO TESTS TITLE}
    Location Should Be    ${url}/systems