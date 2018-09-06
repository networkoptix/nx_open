*** Settings ***
Variables    getvars.py

*** Variables ***
${ALERT}                              //span[@ng-if='!message.compileContent']
${ALERT CLOSE}                        //div[contains(@class, 'ng-toast')]//span[@ng-bind-html='message.content']/../preceding-sibling::button[@ng-click='!message.dismissOnClick && dismiss()']

${BROWSER}                            Chrome

${LANGUAGE DROPDOWN}                  //nx-footer//button[@id='dropdownMenuButton']
${LANGUAGE TO SELECT}                 //nx-footer//span[@lang='${LANGUAGE}']/..
${DOWNLOAD LINK}                      //footer//a[@href="/download"]

@{LANGUAGES LIST}                          en_US    en_GB    ru_RU           fr_FR   de_DE    es_ES   hu_HU  zh_CN  zh_TW  ja_JP    ko_KR   tr_TR  th_TH         nl_NL      he_IL  pl_PL  vi_VN
@{LANGUAGES ACCOUNT TEXT LIST}             Account  Account  Учетная запись  Compte  Account  Cuenta  Fiók   帐户   帳號    アカウント  계정    Hesap   บัญชีผู้ใช้  Account  חשבון    Konto  Tài khoản

${BACKDROP}                           //ngb-modal-window

${CYRILLIC TEXT}                      Кенгшщзх
${SMILEY TEXT}                        ☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★
${GLYPH TEXT}                         您都可以享受源源不絕的好禮及優惠
${SYMBOL TEXT}                        `~!@#$%^&*()_:";'{}[]+<>?,./\
${TM TEXT}                            qweasdzxc123®™

#Log In Elements
${LOG IN MODAL}                       //form[@name='loginForm']
${EMAIL INPUT}                        //form[@name='loginForm']//input[@id='login_email']
${PASSWORD INPUT}                     //form[@name='loginForm']//input[@id='login_password']
${LOG IN BUTTON}                      //form[@name='loginForm']//button[text()= 'Log In']
${REMEMBER ME CHECKBOX}               //form[@name='loginForm']//input[@id='remember']
${FORGOT PASSWORD}                    //form[@name='loginForm']//a[@href='/restore_password']
${LOG IN CLOSE BUTTON}                //button[@data-dismiss='modal']
${ACCOUNT NOT FOUND}                  //form[@name='loginForm']//label[contains(text(), '${ACCOUNT NOT FOUND TEXT}')]
${RESEND ACTIVATION EMAIL LINK}       //form[@name='loginForm']//a[text()='${RESEND ACTIVATION LINK BUTTON TEXT}']
${WRONG PASSWORD MESSAGE}             //form[@name='loginForm']//label[text()="${WRONG PASSWORD}"]
${ACCOUNT NOT FOUND MESSAGE}          //form[@name='loginForm']//label[text()="${ACCOUNT DOES NOT EXIST}"]

${LOG IN NAV BAR}                     //nav//a[contains(@ng-click, 'login()')]
${YOU HAVE NO SYSTEMS}                //span[contains(text(),"${YOU HAVE NO SYSTEMS TEXT}")]

#Header
${ACCOUNT DROPDOWN}                   //header//nx-account-settings-select//button[@id='accountSettingsSelect']
${LOG OUT BUTTON}                     //li[contains(@class, 'collapse-first')]//a[contains(text(), "${LOG OUT BUTTON TEXT}")]
${ACCOUNT SETTINGS BUTTON}            //li//a[@href = '/account/']
${CHANGE PASSWORD BUTTON DROPDOWN}    //li//a[@href = '/account/password/']
${RELEASE HISTORY BUTTON}             //li[contains(@class, 'collapse-first')]//a[contains(text(), "${RELEASE HISTORY BUTTON TEXT}")]
${SYSTEMS DROPDOWN}                   //header//li[contains(@class, 'collapse-second')]//button[@id='systemsDropdown']
${ALL SYSTEMS}                        //header//li[contains(@class, 'collapse-second')]//a[@href='/systems']

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
${RESET SUCCESS MESSAGE}              //h1[contains(text(), "${RESET SUCCESS MESSAGE TEXT}")]
${RESET SUCCESS LOG IN LINK}          //div[@ng-if='change.success || changeSuccess']//a[@href='/login']

#Change Password
${CHANGE PASSWORD FORM}               //form[@name='passwordForm']
${CURRENT PASSWORD INPUT}             //form[@name='passwordForm']//input[@ng-model='pass.password']
${NEW PASSWORD INPUT}                 //form[@name='passwordForm']//password-input[@ng-model='pass.newPassword']//input
${CHANGE PASSWORD BUTTON}             //form[@name='passwordForm']//button[@ng-click='checkForm()']
${PASSWORD IS REQUIRED}               //span[@ng-if='form.passwordNew.$error.required']
${CHANGE PASS EYE ICON OPEN}          ${CHANGE PASSWORD FORM}${EYE ICON OPEN}
${CHANGE PASS EYE ICON CLOSED}        ${CHANGE PASSWORD FORM}${EYE ICON CLOSED}

#Register Form Elements
${REGISTER FORM}                      //form[@name= 'registerForm']
${REGISTER FIRST NAME INPUT}          //form[@name= 'registerForm']//input[@ng-model='account.firstName']
${REGISTER LAST NAME INPUT}           //form[@name= 'registerForm']//input[@ng-model='account.lastName']
${REGISTER EMAIL INPUT}               //form[@name= 'registerForm']//input[@ng-model='account.email']
${REGISTER EMAIL INPUT LOCKED}        //form[@name= 'registerForm']//input['readOnly' and @ng-if='lockEmail']
${REGISTER PASSWORD INPUT}            //form[@name= 'registerForm']//password-input[@ng-model='account.password']//input
${TERMS AND CONDITIONS CHECKBOX}      //form[@name= 'registerForm']//span[@class="checkmark"]
${CREATE ACCOUNT BUTTON}              //form[@name= 'registerForm']//button[contains(text(), "${CREATE ACCOUNT BUTTON TEXT}")]
${TERMS AND CONDITIONS LINK}          //form[@name= 'registerForm']//a[@href='/content/eula']
${TERMS AND CONDITIONS ERROR}         //form[@name= 'registerForm']//span[@ng-if='registerForm.accept.$touched && registerForm.accept.$error.required' and contains(text(), "${TERMS AND CONDITIONS ERROR TEXT}")]
${PRIVACY POLICY LINK}                //form[@name= 'registerForm']//a[@href='${PRIVACY_POLICY_URL}']
${RESEND ACTIVATION LINK BUTTON}      //form[@name= 'loginForm']//a[contains(text(), "${RESEND ACTIVATION LINK BUTTON TEXT}")]
${REGISTER EYE ICON OPEN}             ${REGISTER FORM}${EYE ICON OPEN}
${REGISTER EYE ICON CLOSED}           ${REGISTER FORM}${EYE ICON CLOSED}

${INVITED TO SYSTEM EMAIL SUBJECT UNREGISTERED}    {{message.sharer_name}} invites you to %PRODUCT_NAME%

#Register form errors
${FIRST NAME IS REQUIRED}      //span[@ng-if='registerForm.firstName.$touched && registerForm.firstName.$error.required' and contains(text(),'${FIRST NAME IS REQUIRED TEXT}')]
${LAST NAME IS REQUIRED}       //span[@ng-if='registerForm.lastName.$touched && registerForm.lastName.$error.required' and contains(text(),'${LAST NAME IS REQUIRED TEXT}')]
${EMAIL IS REQUIRED}           //span[@ng-if="registerForm.registerEmail.$touched && registerForm.registerEmail.$error.required" and contains(text(),"${EMAIL IS REQUIRED TEXT}")]
${EMAIL ALREADY REGISTERED}    //span[@ng-if='registerForm.registerEmail.$error.alreadyExists' and contains(text(),'${EMAIL ALREADY REGISTERED TEXT}')]
${EMAIL INVALID}               //span[@ng-if='registerForm.registerEmail.$touched && registerForm.registerEmail.$error.email' and contains(text(),'${EMAIL INVALID TEXT}')]
${PASSWORD SPECIAL CHARS}      //span[contains(@ng-if,'form.passwordNew.$error.pattern &&') and contains(@ng-if,'!form.passwordNew.$error.minlength') and contains(text(),'${PASSWORD SPECIAL CHARS TEXT}')]
${PASSWORD TOO SHORT}          //span[contains(@ng-if,'form.passwordNew.$error.minlength') and contains(text(),'${PASSWORD TOO SHORT TEXT}')]
${PASSWORD TOO COMMON}         //span[contains(@ng-if,'form.passwordNew.$error.common &&') and contains(@ng-if,'!form.passwordNew.$error.required') and contains(text(),'${PASSWORD TOO COMMON TEXT}')]
${PASSWORD IS WEAK}            //span[contains(@ng-if,'form.passwordNew.$error.weak &&') and contains(@ng-if,'!form.passwordNew.$error.common &&') and contains(@ng-if,'!form.passwordNew.$error.pattern &&') and contains(@ng-if,'!form.passwordNew.$error.required &&') and contains(@ng-if,'!form.passwordNew.$error.minlength') and contains(text(),'${PASSWORD IS WEAK TEXT}')]

${INVITED TO SYSTEM EMAIL SUBJECT UNREGISTERED}    {{message.sharer_name}} invites you to %PRODUCT_NAME%

#targets the open nx witness button presented when logging in after activating with from=mobile or client
${OPEN NX WITNESS BUTTON FROM =}      //button[text()="${OPEN NX WITNESS BUTTON TEXT}"]

${EMAIL ALREADY REGISTERED}           //span[@ng-if="registerForm.registerEmail.$error.alreadyExists"]

${ACCOUNT CREATION SUCCESS}           //h1[@ng-if='(register.success || registerSuccess) && !activated']
${ACTIVATION SUCCESS}                 //h1[@ng-if='activate.success' and contains(text(), "${ACCOUNT SUCCESSFULLY ACTIVATED TEXT}")]
${SUCCESS LOG IN BUTTON}              //h1[@ng-if='activate.success' and contains(text(), "${ACCOUNT SUCCESSFULLY ACTIVATED TEXT}")]/following-sibling::h1/a[@href="/login"]
#In system settings
${FIRST USER OWNER}                   //table[@ng-if='system.users.length']/tbody/tr/td[3]/span[contains(text(),"${OWNER TEXT}")]
${DISCONNECT FROM NX}                 //button[@ng-click='disconnect()']
${RENAME SYSTEM}                      //button[@ng-click='rename()']
${RENAME CANCEL}                      //form[@name='renameForm']//button[text()='Cancel']
${RENAME SAVE}                        //form[@name='renameForm']//button[text()='Save']

${RENAME INPUT}                       //form[@name='renameForm']//input[@id='systemName']

${DISCONNECT FROM MY ACCOUNT}         //button[@ng-click='delete()']
${SHARE BUTTON SYSTEMS}               //div[@process-loading='gettingSystem']//button[@ng-click='share()']
${SHARE BUTTON DISABLED}              //div[@process-loading='gettingSystem']//button[@ng-click='share()' and @ng-disabled='!system.isAvailable']
${OPEN IN NX BUTTON}                  //div[@process-loading='gettingSystem']//button[@ng-click='checkForm()']
${OPEN IN NX BUTTON DISABLED}         //div[@process-loading='gettingSystem']//button[@ng-click='checkForm()' and @ng-disabled='buttonDisabled']
${DELETE USER MODAL}                  //div[@uib-modal-transclude]
${DELETE USER BUTTON}                 //button[contains(text(), '${DELETE USER BUTTON TEXT}')]
${DELETE USER CANCEL BUTTON}          //button[@ng-click='cancel()' and contains(text(), "${DELETE USER CANCEL BUTTON TEXT}")]
${SYSTEM NAME OFFLINE}                //span[@ng-if='!system.isOnline']
${USERS LIST}                         //div[@process-loading='gettingSystemUsers']

${SYSTEM NO ACCESS}                   //div[@ng-if='systemNoAccess']/h1[contains(text(), "${SYSTEM NO ACCESS TEXT}")]

#Disconnect from cloud portal
${DISCONNECT FORM}                    //form[@name='disconnectForm']
${DISCONNECT FORM CANCEL}             //form[@name='disconnectForm']//button[text()='Cancel']
${DISCONNECT FORM HEADER}             //h1["${DISCONNECT FORM HEADER TEXT}"]

#Disconnect from my account
${DISCONNECT MODAL WARNING}              //p[contains(text(), "${DISCONNECT MODAL WARNING TEXT}")]
# extra spaces here temporarily
${DISCONNECT MODAL CANCEL}               //button[text()='Cancel ']
${DISCONNECT MODAL DISCONNECT BUTTON}    //button[text()='Disconnect ']

${JUMBOTRON}                          //div[@class='jumbotron']
${PROMO BLOCK}                        //div[contains(@class,'promo-block') and not(contains(@class, 'col-sm-4'))]
${ALREADY ACTIVATED}                  //h1[@ng-if='!activate.success' and contains(text(),"${ALREADY ACTIVATED TEXT}")]

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
${ACCOUNT LANGUAGE DROPDOWN}          //form[@name='accountForm']//nx-language-select//button[@id='dropdownMenuButton']
${ACCOUNT SAVE}                       //form[@name='accountForm']//button[@ng-click='checkForm()']
#Downloads
${DOWNLOADS HEADER}                   //h1["${DOWNLOADS HEADER TEXT}"]
${DOWNLOAD WINDOWS VMS LINK}                  //div[text()="Windows x64 - ${CLIENT ONLY TEXT}"]/../..
${DOWNLOAD UBUNTU VMS LINK}                  //div[text()="Ubuntu x64 - ${CLIENT ONLY TEXT}"]/../..
${DOWNLOAD MAC OS VMS LINK}                  //div[text()="Mac OS X - ${CLIENT ONLY TEXT}"]/../..

${WINDOWS TAB}                        //a[@id="Windows"]
${UBUNTU TAB}                         //a[@id="Linux"]
${MAC OS TAB}                         //a[@id="MacOS"]

#History
${RELEASES TAB}                       //span[@class='tab-heading' and text()='${RELEASES TEXT}']/..
${PATCHES TAB}                        //span[@class='tab-heading' and text()='${PATCHES TEXT}']/..
${BETAS TAB}                          //span[@class='tab-heading' and text()='${BETAS TEXT}']/..
${RELEASE NUMBER}                     //div[contains(@class,"active")]//h1/b

#Misc
${PAGE NOT FOUND}                     //h1[contains(text(), '${PAGE NOT FOUND TEXT}')]
${TAKE ME HOME}                       //a[@href='/' and contains(text(), "${TAKE ME HOME TEXT}")]

${WINDOWS TAB}                        //a[@ng-click="select()"]//span[text()="Windows"]/../..
${UBUNTU TAB}                         //a[@ng-click="select()"]//span[text()="Ubuntu Linux"]/../..
${MAC OS TAB}                         //a[@ng-click="select()"]//span[text()="Mac OS"]/../..

${RELEASE NUMBER}               //div[contains(@class,"active")]//div[@ng-repeat="release in activeBuilds"]//h1/b

#Password badges
${PASSWORD BADGE}                     //span[contains(@class,"badge")]
${PASSWORD TOO SHORT BADGE}           //span[contains(@class,"badge") and contains(text(),'${PASSWORD TOO SHORT BADGE TEXT}')]
${PASSWORD TOO COMMON BADGE}          //span[contains(@class,"badge") and contains(text(),'${PASSWORD TOO COMMON BADGE TEXT}')]
${PASSWORD IS WEAK BADGE}             //span[contains(@class,"badge") and contains(text(),'${PASSWORD IS WEAK BADGE TEXT}')]
${PASSWORD IS FAIR BADGE}             //span[contains(@class,"badge") and contains(text(),'${PASSWORD IS FAIR BADGE TEXT}')]
${PASSWORD IS GOOD BADGE}             //span[contains(@class,"badge") and contains(text(),'${PASSWORD IS GOOD BADGE TEXT}')]
${PASSWORD INCORRECT BADGE}           //span[contains(@class,"badge") and contains(text(),'${PASSWORD INCORRECT BADGE TEXT}')]

#Already logged in modal
#extra spaces here temporarily
${LOGGED IN CONTINUE BUTTON}          //ngb-modal-window//button[text()='Continue ']
${LOGGED IN LOG OUT BUTTON}           //ngb-modal-window//button[text()='Log Out ']

${CONTINUE BUTTON}                    //ngb-modal-window//button[contains(text(), 'Continue')]
${CONTINUE MODAL}                     //ngb-modal-window

${300CHARS}                           QWErtyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmyy
${255CHARS}                           QWErtyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopas

#Eye icons for password forms
${EYE ICON OPEN}             //span[contains(@class, "glyphicon-eye-open")]
${EYE ICON CLOSED}           //span[contains(@class, "glyphicon-eye-close")]

#ASCII
${ESCAPE}                             \\27
${ENTER}                              \\13
${TAB}                                \\9
${SPACEBAR}                           \\32

