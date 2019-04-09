*** Settings ***
Resource          resource.robot
Library           RequestsLibrary

*** variables ***

*** Keywords ***
Bind System
    [Arguments]    ${cloud url}    ${auth}
    &{data}=    Create Dictionary   name=kyle-VirtualBox    customization=default
    Create Digest Session    bind session    ${cloud url}    auth=${auth}
    ${resp}=    Post Request    bind session    /cdb/system/bind    json=${data}
    Should Be Equal As Strings    ${resp.status_code}    200
    Return From Keyword    ${resp.json()}

Setup Cloud System
    [Arguments]    ${server url}    ${auth key}    ${name}    ${id}    ${owner email}    ${auth}
    &{data}=    Create Dictionary    cloudAuthKey=${auth key}    systemName=${name}    cloudSystemID=${id}    cloudAccountName=${owner email}
    Create Digest Session    Setup System session    ${server url}    auth=${auth}
    ${resp}=    Post Request    Setup System session    /api/setupCloudSystem    json=${data}
    Should Be Equal As Strings    ${resp.status_code}    200
    Return From Keyword    ${resp.json()}