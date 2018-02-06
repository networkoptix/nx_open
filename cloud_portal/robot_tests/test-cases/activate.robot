*** Settings ***
Resource          ../resource.robot
Resource          ../variables.robot
Suite Teardown    Close All Browsers

*** Variables ***
${password}    ${BASE PASSWORD}
${url}         ${CLOUD TEST}

*** Test Cases ***
Register and Activate
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    'mark'    'hamill'    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In
    Close Browser

should show error if same link is used twice
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    'mark'    'hamill'    ${email}    ${password}
    ${link}    Get Activation Link    ${email}
    Go To    ${link}
    Wait Until Element Is Visible    ${ACTIVATION SUCCESS}
    Element Should Be Visible    ${ACTIVATION SUCCESS}
    Go To    ${link}
    Wait Until Element Is Visible    ${ALREADY ACTIVATED}
    Element Should Be Visible    ${ALREADY ACTIVATED}
    Close Browser

should save user data to user account correctly
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In
    Go To    ${url}/account
    Wait Until Elements Are Visible    ${ACCOUNT FIRST NAME}    ${ACCOUNT LAST NAME}
    Wait For Condition    return $("form[name='accountForm'] input[ng-model='account.first_name']").val() == 'mark'
    Wait For Condition    return $("form[name='accountForm'] input[ng-model='account.last_name']").val() == 'hamill'
    Close Browser

should allow to enter more than 255 symbols in First and Last names and cut it to 255
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    ${300CHARS}    ${300CHARS}    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In
    Go To    ${url}/account
    Wait Until Elements Are Visible    ${ACCOUNT FIRST NAME}    ${ACCOUNT LAST NAME}
    Wait For Condition    return $("form[name='accountForm'] input[ng-model='account.first_name']").val() == '${255CHARS}'
    Wait For Condition    return $("form[name='accountForm'] input[ng-model='account.last_name']").val() == '${255CHARS}'
    Close Browser

should trim leading and trailing spaces
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    ${SPACE}mark${SPACE}    ${SPACE}hamill${SPACE}    ${email}    ${password}
    Activate    ${email}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In
    Go To    ${url}/account
    Wait For Condition    return $("form[name='accountForm'] input[ng-model='account.first_name']").val() == 'mark'
    Wait For Condition    return $("form[name='accountForm'] input[ng-model='account.last_name']").val() == 'hamill'
    Close Browser

#These are blocked by CLOUD-1624
#should display Open Nx Witness button after activation, if user is registered by link /register/?from=client
#should display Open Nx Witness button after activation, if user is registered by link /register/?from=mobile

link works and suggests to log out user, if he was logged in, buttons operate correctly
    ${email}    Get Random Email
    Open Browser and go to URL    ${url}/register
    Register    mark    hamill    ${email}    ${password}
    ${link}    Get Activation Link    ${email}
    Go To    ${link}
    Log In    ${email}    ${password}    button=${SUCCESS LOG IN BUTTON}
    Validate Log In
    Go To    ${link}
    Wait Until Elements Are Visible    ${LOGGED IN CONTINUE BUTTON}    ${LOGGED IN LOG OUT BUTTON}
    Element Should Be Visible    ${LOGGED IN CONTINUE BUTTON}
    Element Should Be Visible    ${LOGGED IN LOG OUT BUTTON}
    Click Button    ${LOGGED IN CONTINUE BUTTON}
    Validate Log In
    Go To    ${link}
    Wait Until Elements Are Visible    ${LOGGED IN CONTINUE BUTTON}    ${LOGGED IN LOG OUT BUTTON}
    Element Should Be Visible    ${LOGGED IN CONTINUE BUTTON}
    Element Should Be Visible    ${LOGGED IN LOG OUT BUTTON}
    Click Button    ${LOGGED IN LOG OUT BUTTON}
    Validate Log Out
    Close Browser

#This is identical to "redirects to /activate and shows non-activated
#user message when not activated; Resend activation button sends email"
#in login-dialog
email can be sent again
    Open Browser and go to URL    ${url}/register
    ${random email}    get random email
    Register    'mark'    'hamill'    ${random email}    ${BASE PASSWORD}
    Wait Until Element Is Visible    //h1[contains(@class,'process-success')]
    Log In    ${random email}    ${BASE PASSWORD}
    Wait Until Element Is Visible    ${RESEND ACTIVATION LINK BUTTON}
    ${current page}    Get Location
    Should Be True    '${current page}' == '${url}/activate'
    Validate Register Email Received    ${random email}
    Click Button    ${RESEND ACTIVATION LINK BUTTON}
    Validate Register Email Received    ${random email}
    Close Browser