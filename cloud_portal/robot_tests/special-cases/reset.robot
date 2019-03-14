*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers
Test Teardown     Close Browser

*** Variables ***
${password}    ${BASE PASSWORD}
${url}    ${ENV}

*** Test Cases ***
Clean Up
    Clean up email noperm
    Clean up random emails
    Reset user noperm first/last name
    Reset user owner first/last name
    Make sure notowner is in the system
    Reset System Names