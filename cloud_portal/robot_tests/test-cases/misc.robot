*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Teardown     Close Browser

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