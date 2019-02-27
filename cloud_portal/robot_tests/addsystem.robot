*** Settings ***
Resource          ../resource.robot

*** Keywords ***

*** Test Cases ***
Connect system to cloud
    go to web client
    finish server setup with unique name
    add to cloud under owner

Add all required users to system
    verify system is present
    add users under their correct permissions to system