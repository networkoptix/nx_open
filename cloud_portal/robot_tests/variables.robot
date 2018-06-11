*** Settings ***
Variables    getvars.py

*** Variables ***
${ALERT}                              //span[@ng-if='!message.compileContent']
${ALERT CLOSE}                        //div[contains(@class, 'ng-toast')]//span[@ng-bind-html='message.content']/../preceding-sibling::button[@ng-click='!message.dismissOnClick && dismiss()']

${BACKDROP}                           //div[@uib-modal-backdrop="modal-backdrop"]

${BROWSER}                            Chrome

${LANGUAGE DROPDOWN}                  //nx-footer//button[@ngbdropdowntoggle]
${LANGUAGE TO SELECT}                 //nx-footer//span[@lang='${LANGUAGE}']/..

@{LANGUAGES LIST}                          en_US    en_GB    ru_RU           fr_FR   de_DE    es_ES   hu_HU  zh_CN  zh_TW  ja_JP   ko_KR  tr_TR  th_TH     nl_NL    he_IL  pl_PL  vi_VN
@{LANGUAGES ACCOUNT TEXT LIST}             Account  Account  Учетная запись  Compte  Account  Cuenta  Fiók   帐户    帳號   アカウント  계정    Hesap  บัญชีผู้ใช้  Account  חשבון    Konto  Tài khoản

${BACKDROP}                           //ngb-modal-window

${CYRILLIC TEXT}                      Кенгшщзх
${SMILEY TEXT}                        ☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★
${GLYPH TEXT}                         您都可以享受源源不絕的好禮及優惠
${SYMBOL TEXT}                        `~!@#$%^&*()_:";'{}[]+<>?,./\
${TM TEXT}                            qweasdzxc123®™

#Log In Elements
${LOG IN MODAL}                       //form[@name='loginForm']
${EMAIL INPUT}                        //form[@name='loginForm']//input[@id='email']
${PASSWORD INPUT}                     //form[@name='loginForm']//input[@id='password']
${LOG IN BUTTON}                      //form[@name='loginForm']//button[text()= 'Log In']
${REMEMBER ME CHECKBOX}               //form[@name='loginForm']//input[@id='remember']
${FORGOT PASSWORD}                    //form[@name='loginForm']//a[@href='/restore_password']
${LOG IN CLOSE BUTTON}                //button[@data-dismiss='modal']
${ACCOUNT NOT FOUND}                  //form[@name='loginForm']//label[contains(text(), '${ACCOUNT NOT FOUND TEXT}')]
${RESEND ACTIVATION EMAIL LINK}       //form[@name='loginForm']//a[text()='${RESEND ACTVIATION EMAIL}']

${LOG IN NAV BAR}                     //nav//a[contains(@ng-click, 'login()')]
${YOU HAVE NO SYSTEMS}                //span[contains(text(),'${YOU HAVE NO SYSTEMS TEXT}')]

${ACCOUNT DROPDOWN}                   //button[@id = 'accountSettingsSelect']
${LOG OUT BUTTON}                     //li[contains(@class, 'collapse-first')]//a[contains(text(), '${LOG OUT BUTTON TEXT}')]
${ACCOUNT SETTINGS BUTTON}            //li//a[(@href = '/account/')]
${SYSTEMS DROPDOWN}                   //li[contains(@class, 'collapse-second')]//button['btn-dropdown-toggle']
${ALL SYSTEMS}                        //li[contains(@class, 'collapse-second')]//a[@href='/systems']
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
${RESEND ACTIVATION LINK BUTTON}      //form[@name= 'loginForm']//a[contains(text(), "${RESEND ACTIVATION LINK BUTTON TEXT}")]

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
${RENAME CANCEL}                      //form[@name='renameForm']//button[text()='Cancel']
${RENAME SAVE}                        //form[@name='renameForm']//button[text()='Save']
${RENAME INPUT}                       //form[@name='renameForm']//input[@ng-model='model.systemName']
${DISCONNECT FROM MY ACCOUNT}         //button[@ng-click='delete()']
${SHARE BUTTON SYSTEMS}               //div[@process-loading='gettingSystem']//button[@ng-click='share()']
${SHARE BUTTON DISABLED}              //div[@process-loading='gettingSystem']//button[@ng-click='share()' and @ng-disabled='!system.isAvailable']
${OPEN IN NX BUTTON}                  //div[@process-loading='gettingSystem']//button[@ng-click='checkForm()']
${OPEN IN NX BUTTON DISABLED}         //div[@process-loading='gettingSystem']//button[@ng-click='checkForm()' and @ng-disabled='buttonDisabled']
${DELETE USER MODAL}                  //div[@uib-modal-transclude]
${DELETE USER BUTTON}                 //button[contains(text(), '${DELETE USER BUTTON TEXT}')]
${DELETE USER CANCEL BUTTON}          //button[@ng-click='cancel()' and contains(text(), '${DELETE USER CANCEL BUTTON TEXT}')]
${SYSTEM NAME OFFLINE}                //span[@ng-if='!system.isOnline']
${USERS LIST}                         //div[@process-loading='gettingSystemUsers']

${SYSTEM NO ACCESS}                   //div[@ng-if='systemNoAccess']/h1[contains(text(), '${SYSTEM NO ACCESS TEXT}')]

#Disconnect from cloud portal
${DISCONNECT FORM}                    //form[@name='disconnectForm']
${DISCONNECT FORM CANCEL}             //form[@name='disconnectForm']//button[text()='Cancel']
${DISCONNECT FORM HEADER}             //h1['${DISCONNECT FORM HEADER TEXT}']

#Disconnect from my account
${DISCONNECT MODAL WARNING}              //p[contains(text(), '${DISCONNECT MODAL WARNING TEXT}')]
# extra spaces here temporarily
${DISCONNECT MODAL CANCEL}               //button[text()='Cancel ']
${DISCONNECT MODAL DISCONNECT BUTTON}    //button[text()='Disconnect ']

${JUMBOTRON}                          //div[@class='jumbotron']
${PROMO BLOCK}                        //div[contains(@class,'promo-block') and not(contains(@class, 'col-sm-4'))]
${ALREADY ACTIVATED}                  //h1[@ng-if='!activate.success' and contains(text(),'${ALREADY ACTIVATED TEXT}')]

#Share Elements (Note: Share and Permissions are the same form so these are the same variables.  Making two just in case they do diverge at some point.)
${SHARE MODAL}                        //form[@name='shareForm']
${SHARE EMAIL}                        //form[@name='shareForm']//input[@id='email']
${SHARE PERMISSIONS DROPDOWN}         //form[@name='shareForm']//nx-permissions-select//button[@id='permissionsSelect']
${SHARE BUTTON MODAL}                 //form[@name='shareForm']//button[text()='Share']
${SHARE CANCEL}                       //form[@name='shareForm']//button[text()='Cancel']
${SHARE CLOSE}                        //form[@name='shareForm']//button[@data-dismiss='modal']
${SHARE PERMISSIONS HINT}             //form[@name='shareForm']//span[contains(@class,'help-block')]

${EDIT PERMISSIONS EMAIL}             //form[@name='shareForm']//input[@ng-model='user.email']
${EDIT PERMISSIONS DROPDOWN}          //form[@name='shareForm']//select[@ng-model='user.role']
${EDIT PERMISSIONS SAVE}              //form[@name='shareForm']//button[text()='Save']
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
${ACCOUNT LANGUAGE DROPDOWN}          //form[@name='accountForm']//nx-language-select//button[@id = 'languageSelect']
${ACCOUNT SAVE}                       //form[@name='accountForm']//button[@ng-click='checkForm()']
${ACCOUNT SUBSCRIBE CHECKBOX}         //form[@name='accountForm']//input[@ng-model='account.subscribe']/following-sibling::span[contains(@class, 'checkmark')]
#Already logged in modal
#extra spaces here temporarily
${LOGGED IN CONTINUE BUTTON}          //ngb-modal-window//button[text()='Continue ']
${LOGGED IN LOG OUT BUTTON}           //ngb-modal-window//button[text()='Log Out ']

${CONTINUE BUTTON}                    //ngb-modal-window//button[contains(text(), 'Continue')]
${CONTINUE MODAL}                     //ngb-modal-window

${300CHARS}                           QWErtyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmyy
${255CHARS}                           QWErtyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopas

#ASCII
${ESCAPE}                             \\27
${ENTER}                              \\13
${TAB}                                \\9
${SPACEBAR}                           \\32