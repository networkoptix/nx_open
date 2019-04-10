*** Settings ***
Resource          resource.robot
Library           RequestsLibrary

*** variables ***

*** Keywords ***
Bind System
    [Arguments]    ${auth}    ${cloud url}
    &{data}=    Create Dictionary   name=kyle-VirtualBox    customization=default
    Create Digest Session    bind session    ${cloud url}    auth=${auth}
    ${resp}=    Post Request    bind session    /cdb/system/bind    json=${data}
    Should Be Equal As Strings    ${resp.status_code}    200
    Return From Keyword    ${resp.json()}

Setup Cloud System
    [Arguments]    ${auth}    ${server url}    ${auth key}    ${name}    ${id}    ${owner email}
    &{data}=    Create Dictionary    cloudAuthKey=${auth key}    systemName=${name}    cloudSystemID=${id}    cloudAccountName=${owner email}
    Create Digest Session    Setup System session    ${server url}    auth=${auth}
    ${resp}=    Post Request    Setup System session    /api/setupCloudSystem    json=${data}
    Should Be Equal As Strings    ${resp.status_code}    200
    Return From Keyword    ${resp.json()}

Save User Role
    [Arguments]    ${auth}    ${server url}    ${name}    ${permissions}
    &{data}=    Create Dictionary    name=${name}    permissions=${permissions}
    Create Digest Session    Save User Role session    ${server url}    auth=${auth}
    ${resp}=    Post Request    Save User Role session    /ec2/saveUserRole    json=${data}
    Should Be Equal As Strings    ${resp.status_code}    200
    Return From Keyword    ${resp.json()}

Share
    [Arguments]    ${auth}    ${cloud url}    ${system id}    ${access role}    ${account email}
    &{data}=    Create Dictionary    systemId=${system id}    accessRole=${access role}    accountEmail=${account email}
    Create Digest Session    Share session    ${cloud url}    auth=${auth}
    ${resp}=    Post Request    Share session    /cdb/system/share    json=${data}
    Should Be Equal As Strings    ${resp.status_code}    200
    Return From Keyword    ${resp.json()}

Get Users
    [Arguments]    ${auth}    ${server url}
    Create Digest Session    Get Users session   ${server url}    auth=${auth}
    ${resp}=    Get Request    Get Users session    /ec2/getUsers
    Should Be Equal As Strings    ${resp.status_code}    200
    Return From Keyword    ${resp.json()}

Save User
    [Arguments]    ${auth}    ${server url}    ${user id}    ${user role id}
    &{data}=    Create Dictionary    isCloud=${true}    id=${user id}    userRoleId=${user role id}
    Create Digest Session    Save User session    ${server url}    auth=${auth}
    ${resp}=    Post Request    Save User session    /ec2/saveUser    json=${data}
    Should Be Equal As Strings    ${resp.status_code}    200
    Return From Keyword    ${resp.json()}