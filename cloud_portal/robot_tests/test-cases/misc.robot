*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Test Teardown     Close Browser

*** Variables ***
${url}    ${ENV}
*** Keywords ***


*** Test Cases ***
404 page shows when going to a url that doesn't exist and gives a link back to home page
    Open Browser and go to URL    ${url}/wfvyuieyuisgweyugv
    Wait Until Elements Are Visible    //h1[contains(text(), 'Page not found')]    //a[@href='/' and contains(text(), 'Take me home')]

Failed to access system page correctly shows when going to a non-existent system
    Open Browser and go to URL    ${url}
    Log In    ${EMAIL OWNER}    ${BASE PASSWORD}
    Validate Log In
    Go To    ${url}/systems/htgfjtrdtrtyrrtydrydcrtydrtrdrtdrtdrtdrtd
    Wait Until Elements Are Visible    //h1['Failed to access System']    //p[contains(text(), 'This link is broken,')]    //p//a[@href='/systems']/..
    ${text}    Get Text    //p[contains(text(), 'This link is broken,')]
    log    ${text}
    Should Contain    ${text}    This link is broken,
    Should Contain    ${text}    System may be disconnected from Nx Cloud,
    Should Contain    ${text}    Access to this System may be revoked,
    Should Contain    ${text}    or you may be logged into a different account