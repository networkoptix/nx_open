*** Settings ***
Variables    getvars.py

*** Variables ***
${ALERT}                              //div[contains(@class, 'ng-toast')]//span[@ng-bind-html='message.content']
${ALERT CLOSE}                        //div[contains(@class, 'ng-toast')]//span[@ng-bind-html='message.content']/../preceding-sibling::button[@ng-click='!message.dismissOnClick && dismiss()']

${LOCAL}                              https://localhost:9000/
${CLOUD TEST}                         https://cloud-test.hdw.mx
${CLOUD DEV}                          https://cloud-dev2.hdw.mx
${CLOUD TEST REGISTER}                https://cloud-test.hdw.mx/register
${CLOUD STAGE}                        https://cloud-stage.hdw.mx
${ENV}                                ${CLOUD TEST}
${SCREENSHOTDIRECTORY}               \Screenshots

${BROWSER}                            Chrome

${BACKDROP}                           //div[@uib-modal-backdrop="modal-backdrop"]

${LANGUAGE DROPDOWN}                  //footer//button[@uib-dropdown-toggle]
${LANGUAGE TO SELECT}                 //footer//span[@lang='${LANGUAGE}']/..

@{LANGUAGES LIST}                          en_US    en_GB    ru_RU           fr_FR   de_DE    es_ES   hu_HU  zh_CN  zh_TW  ja_JP   ko_KR  tr_TR  th_TH     nl_NL    he_IL  pl_PL  vi_VN
@{LANGUAGES ACCOUNT TEXT LIST}             Account  Account  Учетная запись  Compte  Account  Cuenta  Fiók   帐户    帳號   アカウント  계정    Hesap  บัญชีผู้ใช้  Account  חשבון    Konto  Tài khoản



${CYRILLIC TEXT}                      Кенгшщзх
${SMILEY TEXT}                        ☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★
${GLYPH TEXT}                         您都可以享受源源不絕的好禮及優惠
${SYMBOL TEXT}                        `~!@#$%^&*()_:";'{}[]+<>?,./\
${TM TEXT}                            qweasdzxc123®™

#Log In Elements
${LOG IN MODAL}                       //div[contains(@class, 'modal-content')]
${EMAIL INPUT}                        //form[contains(@name, 'loginForm')]//input[@ng-model='auth.email']
${PASSWORD INPUT}                     //form[contains(@name, 'loginForm')]//input[@ng-model='auth.password']
${LOG IN BUTTON}                      //form[contains(@name, 'loginForm')]//button[@ng-click='checkForm()']
${REMEMBER ME CHECKBOX}               //form[contains(@name, 'loginForm')]//input[@ng-model='auth.remember']
${FORGOT PASSWORD}                    //form[contains(@name, 'loginForm')]//a[@href='/restore_password']
${LOG IN CLOSE BUTTON}                //button[@ng-click='close()']

${LOG IN NAV BAR}                     //nav//a[contains(@ng-click, 'login()')]
${YOU HAVE NO SYSTEMS}                //span[contains(text(),'${YOU HAVE NO SYSTEMS TEXT}')]

${ACCOUNT DROPDOWN}                   //li[contains(@class, 'collapse-first')]//a['uib-dropdown-toggle']
${LOG OUT BUTTON}                     //li[contains(@class, 'collapse-first')]//a[contains(text(), '${LOG OUT BUTTON TEXT}')]
${ACCOUNT SETTINGS BUTTON}            //li[contains(@class, 'collapse-first')]//a[contains(text(), '${ACCOUNT SETTINGS BUTTON TEXT}')]
${SYSTEMS DROPDOWN}                   //li[contains(@class, 'collapse-second')]//a['uib-dropdown-toggle']
${ALL SYSTEMS}                        //li[contains(@class, 'collapse-second')]//a[@ng-href='/systems']
${AUTHORIZED BODY}                    //body[contains(@class, 'authorized')]
${ANONYMOUS BODY}                     //body[contains(@class,'anonymous')]
${CREATE ACCOUNT HEADER}              //header//a[@href='/register']
${CREATE ACCOUNT BODY}                //body//a[@href='/register']

#Forgot Password
${RESTORE PASSWORD EMAIL INPUT}       //form[@name='restorePassword']//input[@type='email']
${RESET PASSWORD BUTTON}              //form[@name='restorePassword']//button[@ng-click='checkForm()']
${RESET PASSWORD INPUT}               //form[@name='restorePasswordWithCode']//input[@type='password']
${SAVE PASSWORD}                      //form[@name='restorePasswordWithCode']//button[@ng-click='checkForm()']
${RESET EMAIL SENT MESSAGE}           //div[@ng-if='restoringSuccess']/h1
${RESET SUCCESS MESSAGE}              //h1[contains(text(), '${RESET SUCCESS MESSAGE TEXT}')]
${RESET SUCCESS LOG IN LINK}          //h1[@ng-if='change.success || changeSuccess']//a[@href='/login']

#Change Password
${CURRENT PASSWORD INPUT}             //form[@name='passwordForm']//input[@ng-model='pass.password']
${NEW PASSWORD INPUT}                 //form[@name='passwordForm']//password-input[@ng-model='pass.newPassword']//input[@type='password']
${CHANGE PASSWORD BUTTON}             //form[@name='passwordForm']//button[@ng-click='checkForm()']
${PASSWORD IS REQUIRED}               //span[@ng-if='passwordInput.password.$error.required']

#Register Form Elements
${REGISTER FIRST NAME INPUT}          //form[@name= 'registerForm']//input[@ng-model='account.firstName']
${REGISTER LAST NAME INPUT}           //form[@name= 'registerForm']//input[@ng-model='account.lastName']
${REGISTER EMAIL INPUT}               //form[@name= 'registerForm']//input[@ng-model='account.email']
${REGISTER EMAIL INPUT LOCKED}        //form[@name= 'registerForm']//input['readOnly' and @ng-if='lockEmail']
${REGISTER PASSWORD INPUT}            //form[@name= 'registerForm']//password-input[@ng-model='account.password']//input[@type='password']
${TERMS AND CONDITIONS CHECKBOX}      //form[@name= 'registerForm']//input[@ng-model='account.accept']
${CREATE ACCOUNT BUTTON}              //form[@name= 'registerForm']//button[contains(text(), '${CREATE ACCOUNT BUTTON TEXT}')]
${TERMS AND CONDITIONS LINK}          //form[@name= 'registerForm']//a[@href='/content/eula']
${TERMS AND CONDITIONS ERROR}         //form[@name= 'registerForm']//p[@ng-if='registerForm.accept.$touched && registerForm.accept.$error.required' and contains(text(), '${TERMS AND CONDITIONS ERROR TEXT}')]
${PRIVACY POLICY LINK}                //form[@name= 'registerForm']//a[@href='/content/support']
${RESEND ACTIVATION LINK BUTTON}      //form[@name= 'reactivateAccount']//button[contains(text(), "${RESEND ACTIVATION LINK BUTTON TEXT}")]

#targets the open nx witness button presented when logging in after activating with from=mobile or client
${OPEN NX WITNESS BUTTON FROM =}      //button[text()='${OPEN NX WITNESS BUTTON TEXT}']

${EMAIL ALREADY REGISTERED}           //span[@ng-if="registerForm.registerEmail.$error.alreadyExists"]

${ACCOUNT CREATION SUCCESS}           //h1[@ng-if='(register.success || registerSuccess) && !activated']
${ACTIVATION SUCCESS}                 //h1[@ng-if='activate.success' and contains(text(), '${ACCOUNT SUCCESSFULLY ACTIVATED TEXT}')]
${SUCCESS LOG IN BUTTON}              //h1[@ng-if='activate.success' and contains(text(), '${ACCOUNT SUCCESSFULLY ACTIVATED TEXT}')]/following-sibling::h1/a[@href="/login"]
#In system settings
${FIRST USER OWNER}                   //table[@ng-if='system.users.length']/tbody/tr/td[3]/span[contains(text(),'${OWNER TEXT}')]
${DISCONNECT FROM NX}                 //button[@ng-click='disconnect()']
${RENAME SYSTEM}                      //button[@ng-click='rename()']
${RENAME CANCEL}                      //form[@name='renameForm']//button[@ng-click='close()']
${RENAME SAVE}                        //form[@name='renameForm']//button[@ng-click='checkForm()']
${RENAME INPUT}                       //form[@name='renameForm']//input[@ng-model='model.systemName']
${DISCONNECT FROM MY ACCOUNT}         //button[@ng-click='delete()']
${SHARE BUTTON SYSTEMS}               //div[@process-loading='gettingSystem']//button[@ng-click='share()']
${SHARE BUTTON DISABLED}              //div[@process-loading='gettingSystem']//button[@ng-click='share()' and @ng-disabled='!system.isAvailable']
${OPEN IN NX BUTTON}                  //div[@process-loading='gettingSystem']//button[@ng-click='checkForm()']
${OPEN IN NX BUTTON DISABLED}         //div[@process-loading='gettingSystem']//button[@ng-click='checkForm()' and @ng-disabled='buttonDisabled']
${DELETE USER MODAL}                  //div[@uib-modal-transclude]
${DELETE USER BUTTON}                 //button[@ng-click='ok()' and contains(text(), '${DELETE USER BUTTON TEXT}')]
${DELETE USER CANCEL BUTTON}          //button[@ng-click='cancel()' and contains(text(), '${DELETE USER CANCEL BUTTON TEXT}')]
${SYSTEM NAME OFFLINE}                //span[@ng-if='!system.isOnline']
${USERS LIST}                         //div[@process-loading='gettingSystemUsers']

${SYSTEM NO ACCESS}                   //div[@ng-if='systemNoAccess']/h1[contains(text(), '${SYSTEM NO ACCESS TEXT}')]

#Disconnect from cloud portal
${DISCONNECT FORM}                    //form[@name='disconnectForm']
${DISCONNECT FORM CANCEL}             //form[@name='disconnectForm']//button[@ng-click='close()']
${DISCONNECT FORM HEADER}             //h1['${DISCONNECT FORM HEADER TEXT}']

#Disconnect from my account
${DISCONNECT MODAL WARNING}              //p[contains(text(), '${DISCONNECT MODAL WARNING TEXT}')]
${DISCONNECT MODAL CANCEL}               //button[@ng-click='cancel()']
${DISCONNECT MODAL DISCONNECT BUTTON}    //button[@ng-click='ok()']

${JUMBOTRON}                          //div[@class='jumbotron']
${PROMO BLOCK}                        //div[contains(@class,'promo-block') and not(contains(@class, 'col-sm-4'))]
${ALREADY ACTIVATED}                  //h1[@ng-if='!activate.success' and contains(text(),'${ALREADY ACTIVATED TEXT}')]

#Share Elements (Note: Share and Permissions are the same form so these are the same variables.  Making two just in case they do diverge at some point.)
${SHARE MODAL}                        //div[@uib-modal-transclude]
${SHARE EMAIL}                        //form[@name='shareForm']//input[@ng-model='user.email']
${SHARE PERMISSIONS DROPDOWN}         //form[@name='shareForm']//select[@ng-model='user.role']
${SHARE BUTTON MODAL}                 //form[@name='shareForm']//button[@ng-click='checkForm()']
${SHARE CANCEL}                       //form[@name='shareForm']//button[@ng-click='close()']
${SHARE CLOSE}                        //div[@uib-modal-transclude]//div[@ng-if='settings.title']//button[@ng-click='close()']
${SHARE PERMISSIONS HINT}             //form[@name='shareForm']//span[contains(@class,'help-block')]

${EDIT PERMISSIONS EMAIL}             //form[@name='shareForm']//input[@ng-model='user.email']
${EDIT PERMISSIONS DROPDOWN}          //form[@name='shareForm']//select[@ng-model='user.role']
${EDIT PERMISSIONS SAVE}              //form[@name='shareForm']//button[@ng-click='checkForm()']
${EDIT PERMISSIONS CANCEL}            //form[@name='shareForm']//button[@ng-click='close()']
${EDIT PERMISSIONS CLOSE}             //div[@uib-modal-transclude]//div[@ng-if='settings.title']//button[@ng-click='close()']
${EDIT PERMISSIONS ADMINISTRATOR}     //form[@name='shareForm']//select[@ng-model='user.role']//option[@label='Administrator']
${EDIT PERMISSIONS ADVANCED VIEWER}   //form[@name='shareForm']//select[@ng-model='user.role']//option[@label='Advanced Viewer']
${EDIT PERMISSIONS VIEWER}            //form[@name='shareForm']//select[@ng-model='user.role']//option[@label='Viewer']
${EDIT PERMISSIONS LIVE VIEWER}       //form[@name='shareForm']//select[@ng-model='user.role']//option[@label='Live Viewer']
${EDIT PERMISSIONS CUSTOM}            //form[@name='shareForm']//select[@ng-model='user.role']//option[@label='Custom']
${EDIT PERMISSIONS HINT}              //form[@name='shareForm']//span[contains(@class,'help-block')]

#Account Page
${ACCOUNT EMAIL}                      //form[@name='accountForm']//input[@ng-model='account.email']
${ACCOUNT FIRST NAME}                 //form[@name='accountForm']//input[@ng-model='account.first_name']
${ACCOUNT LAST NAME}                  //form[@name='accountForm']//input[@ng-model='account.last_name']
${ACCOUNT LANGUAGE DROPDOWN}          //form[@name='accountForm']//language-select//button
${ACCOUNT SAVE}                       //form[@name='accountForm']//button[@ng-click='checkForm()']
${ACCOUNT SUBSCRIBE CHECKBOX}         //form[@name='accountForm']//input[@ng-model='account.subscribe']

#Already logged in modal
${LOGGED IN CONTINUE BUTTON}          //div[@uib-modal-transclude]//button[@ng-click='ok()']
${LOGGED IN LOG OUT BUTTON}           //div[@uib-modal-transclude]//button[@ng-click='cancel()']

${CONTINUE BUTTON}                    //div[@uib-modal-window='modal-window']//button[@ng-class='settings.buttonClass' and @ng-click='ok()']
${CONTINUE MODAL}                     //div[@uib-modal-window='modal-window']

${300CHARS}                           QWErtyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmyy
${255CHARS}                           QWErtyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopas

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
${AUTO_TESTS SYSTEM ID}                     9a4c9037-f265-436c-a0c6-8ef714465528
${AUTO TESTS TITLE}                   //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[text()='Auto Tests']
${AUTO TESTS USER}                    //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[text()='Auto Tests']/following-sibling::span[contains(@class,'user-name')]
${AUTO TESTS OPEN NX}                 //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[text()='Auto Tests']/..//button[@ng-click='checkForm()']
${SYSTEMS SEARCH INPUT}               //input[@ng-model='search.value']
${SYSTEMS TILE}                       //div[@ng-repeat="system in systems | filter:searchSystems as filtered"]
${NOT OWNER IN SYSTEM}                //div[@process-loading='gettingSystemUsers']//tbody//tr//td[contains(text(), '${EMAIL NOT OWNER}')]

#AUTO TESTS 2 is an offline system used for testing offline status on the systems page and offline status on the system page
${AUTOTESTS OFFLINE}                  //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[contains(text(),'Auto Tests 2')]/following-sibling::span[contains(text(), '${AUTOTESTS OFFLINE TEXT}')]
${AUTOTESTS OFFLINE OPEN NX}          //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[contains(text(),'Auto Tests 2')]/..//button[@ng-click='checkForm()']
${AUTOTESTS OFFLINE SYSTEM ID}                6f53b2b9-422f-48d7-87b1-b82c032e7ba0

#ASCII
${ESCAPE}                             \\27
${ENTER}                              \\13
${TAB}                                \\9
${SPACEBAR}                           \\32