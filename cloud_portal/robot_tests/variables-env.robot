*** Variables ***

${LOCAL}                              https://localhost:9000/
${CLOUD TEST}                         https://cloud-test.hdw.mx
${CLOUD DEV}                          https://cloud-dev2.hdw.mx
${CLOUD TEST REGISTER}                https://cloud-test.hdw.mx/register
${CLOUD STAGE}                        https://cloud-stage.hdw.mx
${ENV}                                ${CLOUD TEST}
${SCREENSHOTDIRECTORY}               \Screenshots

${BROWSER}                            Chrome

#Emails
${BASE EMAIL}                         noptixautoqa@gmail.com
${BASE EMAIL PASSWORD}                qweasd!@#$%
${BASE HOST}                          imap.gmail.com
${BASE PORT}                          993
${EMAIL VIEWER}                       noptixautoqa+viewer@gmail.com
${EMAIL ADV VIEWER}                   noptixautoqa+advviewer@gmail.com
${EMAIL LIVE VIEWER}                  noptixautoqa+liveviewer@gmail.com
${EMAIL OWNER}                        noptixautoqa+owner@gmail.com
${EMAIL NOT OWNER}                    noptixautoqa+notowner@gmail.com
${EMAIL ADMIN}                        noptixautoqa+admin@gmail.com
${EMAIL CUSTOM}                       noptixautoqa+custom@gmail.com
@{EMAILS LIST}                        ${EMAIL VIEWER}    ${EMAIL ADV VIEWER}    ${EMAIL LIVE VIEWER}    ${EMAIL OWNER}    ${EMAIL ADMIN}    ${EMAIL CUSTOM}
${ADMIN FIRST NAME}                   mark
${ADMIN LAST NAME}                    hamil
${EMAIL UNREGISTERED}                 noptixautoqa+unregistered@gmail.com
${EMAIL NOPERM}                       noptixautoqa+noperm@gmail.com
${BASE PASSWORD}                      qweasd123
${ALT PASSWORD}                       qweasd1234

${TEST FIRST NAME}                    testFirstName
${TEST LAST NAME}                     testLastName

#Related to Auto Tests system
${AUTO TESTS}                         Auto Tests
${AUTO_TESTS SYSTEM ID}                     69e29599-ffe5-482a-ab0f-35ebce3b0deb
${AUTO TESTS TITLE}                   //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[text()='Auto Tests']
${AUTO TESTS USER}                    //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[text()='Auto Tests']/following-sibling::span[contains(@class,'user-name')]
${AUTO TESTS OPEN NX}                 //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[text()='Auto Tests']/..//button[@ng-click='checkForm()']
${SYSTEMS SEARCH INPUT}               //input[@ng-model='search.value']
${SYSTEMS TILE}                       //div[@ng-repeat="system in systems | filter:searchSystems as filtered"]
${NOT OWNER IN SYSTEM}                //div[@process-loading='gettingSystemUsers']//tbody//tr//td[contains(text(), '${EMAIL NOT OWNER}')]

#AUTO TESTS 2 is an offline system used for testing offline status on the systems page and offline status on the system page
${AUTOTESTS OFFLINE}                  //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[contains(text(),'Auto Tests 2')]/following-sibling::span[contains(text(), '${AUTOTESTS OFFLINE TEXT}')]
${AUTOTESTS OFFLINE OPEN NX}          //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[contains(text(),'Auto Tests 2')]/..//button[@ng-click='checkForm()']
${AUTOTESTS OFFLINE SYSTEM ID}                9551a722-6ea2-4d0b-91ae-5ac104c9a413