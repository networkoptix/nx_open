*** Variables ***

${LOCAL}                              https://localhost:9000/
${CLOUD TEST}                         https://cloud-test.hdw.mx
${CLOUD DEV}                          https://cloud-dev2.hdw.mx
${CLOUD TEST REGISTER}                https://cloud-test.hdw.mx/register
${CLOUD STAGE}                        https://cloud-stage.hdw.mx
${DOWNLOADS DOMAIN}                   updates.networkoptix.com
${ENV}                                ${CLOUD TEST}
${SCREENSHOTDIRECTORY}                \Screenshots

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
${BASE PASSWORD}                      qweasd 123
${ALT PASSWORD}                       qweasd1234

${TEST FIRST NAME}                    testFirstName
${TEST LAST NAME}                     testLastName

#Related to Auto Tests system
${AUTO TESTS}                         Auto Tests
${AUTO_TESTS SYSTEM ID}                     813f5aa8-10bd-4fa8-a5f4-e276a39f80df
${AUTO TESTS TITLE}                   //div[@ng-repeat='system in systems | filter:searchSystems as filtered track by system.id']//h2[text()='Auto Tests']
${AUTO TESTS USER}                    //div[@ng-repeat='system in systems | filter:searchSystems as filtered track by system.id']//h2[text()='Auto Tests']/following-sibling::span[contains(@class,'user-name')]
${AUTO TESTS OPEN NX}                 //div[@ng-repeat='system in systems | filter:searchSystems as filtered track by system.id']//h2[text()='Auto Tests']/..//button[@ng-click='checkForm()']
${SYSTEM NAME AUTO TESTS HEADER}      //header//li/a/span[text()="Auto Tests"]
${SYSTEMS SEARCH INPUT}               //input[@ng-model='search.value']
${SYSTEMS TILE}                       //div[@ng-repeat="system in systems | filter:searchSystems as filtered track by system.id"]
${NOT OWNER IN SYSTEM}                //div[@process-loading='gettingSystemUsers']//tbody//tr//td[contains(text(), '${EMAIL NOT OWNER}')]

#AUTO TESTS 2 is an offline system used for testing offline status on the systems page and offline status on the system page
${AUTO TESTS 2}                       Auto Tests 2
${AUTOTESTS OFFLINE}                  //div[@ng-repeat='system in systems | filter:searchSystems as filtered track by system.id']//h2[contains(text(),'Auto Tests 2')]/following-sibling::span[contains(text(), '${AUTOTESTS OFFLINE TEXT}')]
${AUTOTESTS OFFLINE OPEN NX}          //div[@ng-repeat='system in systems | filter:searchSystems as filtered track by system.id']//h2[contains(text(),'Auto Tests 2')]/..//button[@ng-click='checkForm()']
${AUTOTESTS OFFLINE SYSTEM ID}                aee1df1e-ea9d-43c8-9c04-0463758d3616

${OUTLINE ERROR COLOR}                rgb(217, 42, 42)