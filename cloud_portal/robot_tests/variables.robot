*** Variables ***
${ALERT}                              //div[contains(@class, 'ng-toast')]//span[@ng-bind-html='message.content']
${ALERT CLOSE}                        //div[contains(@class, 'ng-toast')]//span[@ng-bind-html='message.content']/../preceding-sibling::button[@ng-click='!message.dismissOnClick && dismiss()']

${LOCAL}                              https://localhost:9000/

${CLOUD TEST}                         https://cloud-test.hdw.mx
${CLOUD DEV}                          https://cloud-dev2.hdw.mx
${CLOUD TEST REGISTER}                https://cloud-test.hdw.mx/register

${CYRILLIC NAME}                     Кенгшщзх
${SMILEY NAME}                       ☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★
${GLYPH NAME}                        您都可以享受源源不絕的好禮及優惠
${SYMBOL NAME}                       `~!@#$%^&*()_:";'{}[]+<>?,./\

#Log In Elements
${LOG IN MODAL}                       //div[contains(@class, 'modal-content')]
${EMAIL INPUT}                        //form[contains(@name, 'loginForm')]//input[@ng-model='auth.email']
${PASSWORD INPUT}                     //form[contains(@name, 'loginForm')]//input[@ng-model='auth.password']
${LOG IN BUTTON}                      //form[contains(@name, 'loginForm')]//button[@ng-click='checkForm()']
${REMEMBER ME CHECKBOX}               //form[contains(@name, 'loginForm')]//input[@ng-model='auth.remember']
${FORGOT PASSWORD}                    //form[contains(@name, 'loginForm')]//a[@href='/restore_password']
${LOG IN CLOSE BUTTON}                //button[@ng-click='close()']

${LOG IN NAV BAR}                     //nav//a[contains(@ng-click, 'login()')]
${YOU HAVE NO SYSTEMS}                //span[contains(text(),'You have no Systems connected to Nx Cloud')]

${ACCOUNT DROPDOWN}                   //li[contains(@class, 'collapse-first')]//a['uib-dropdown-toggle']
${LOG OUT BUTTON}                     //li[contains(@class, 'collapse-first')]//a[contains(text(), 'Log Out')]
${AUTHORIZED BODY}                    //body[contains(@class, 'authorized')]
${ANONYMOUS BODY}                     //body[contains(@class,'anonymous')]
${CREATE ACCOUNT HEADER}              //header//a[@href='/register']
${CREATE ACCOUNT BODY}                //body//a[@href='/register']

#Forgot Password
${RESTORE PASSWORD EMAIL INPUT}       //form[@name='restorePassword']//input[@type='email']
${RESET PASSWORD BUTTON}              //form[@name='restorePassword']//button[@ng-click='checkForm()']
${RESET PASSWORD INPUT}               //form[@name='restorePasswordWithCode']//input[@type='password']
${SAVE PASSWORD}                      //form[@name='restorePasswordWithCode']//button[@ng-click='checkForm()']
${RESET EMAIL SENT MESSAGE}           //div[@ng-if='restoringSuccess']/h1[contains(text(),'Password reset instructions are sent to Email')]
${RESET SUCCESS MESSAGE}              //h1[@ng-if='change.success || changeSuccess ' and contains(text(), 'Password successfully saved')]
${RESET SUCCESS LOG IN LINK}          //h1[@ng-if='change.success || changeSuccess ' and contains(text(), 'Password successfully saved')]/a[@href='/login']

#Change Password
${CURRENT PASSWORD INPUT}             //form[@name='passwordForm']//input[@ng-model='pass.password']
${NEW PASSWORD INPUT}                 //form[@name='passwordForm']//password-input[@ng-model='pass.newPassword']//input[@type='password']
${CHANGE PASSWORD BUTTON}             //form[@name='passwordForm']//button[@ng-click='checkForm()']

#Register Form Elements
${REGISTER FIRST NAME INPUT}          //form[@name= 'registerForm']//input[@ng-model='account.firstName']
${REGISTER LAST NAME INPUT}           //form[@name= 'registerForm']//input[@ng-model='account.lastName']
${REGISTER EMAIL INPUT}               //form[@name= 'registerForm']//input[@ng-model='account.email']
${REGISTER PASSWORD INPUT}            //form[@name= 'registerForm']//password-input[@ng-model='account.password']//input[@type='password']
${REGISTER SUBSCRIBE CHECKBOX}        //form[@name= 'registerForm']//input[@ng-model='account.subscribe']
${CREATE ACCOUNT BUTTON}              //form[@name= 'registerForm']//button[contains(text(), 'Create Account')]
${TERMS AND CONDITIONS LINK}          //form[@name= 'registerForm']//a[@href='/content/eula' and contains(text(), 'Terms and Conditions')]
${RESEND ACTIVATION LINK BUTTON}      //form[@name= 'reactivateAccount']//button[contains(text(), 'Resend activation link')]

${ACCOUNT CREATION SUCCESS}           //h1[@ng-if='(register.success || registerSuccess) && !activated']
${ACTIVATION SUCCESS}                 //div[@ng-model-options="{ updateOn: 'blur' }"]//h1[@ng-if='activate.success' and contains(text(), 'Your account is successfully activated')]
${SUCCESS LOG IN BUTTON}              //div[@ng-model-options="{ updateOn: 'blur' }"]//h1[@ng-if='activate.success' and contains(text(), 'Your account is successfully activated')]//a[@href='/login']

#In system settings
${DISCONNECT FROM NX}                 //button[@ng-click='disconnect()']
${RENAME SYSTEM}                      //button[@ng-click='rename()']
${DISCONNECT FROM MY ACCOUNT}         //button[@ng-click='delete()']
${SHARE BUTTON SYSTEMS}               //div[@process-loading='gettingSystem']//button[@ng-click='share()']
${OPEN IN NX BUTTON}                  //div[@process-loading='gettingSystem']//button[@ng-click='checkForm()']
${DELETE USER MODAL}                  //div[@uib-modal-transclude]
${DELETE USER BUTTON}                 //button[@ng-click='ok()' and contains(text(), 'Delete')]
${DELETE USER CANCEL BUTTON}          //button[@ng-click='cancel()' and contains(text(), 'Cancel')]

${SYSTEM NO ACCESS}                   //div[@ng-if='systemNoAccess']/h1[contains(text(), 'Failed to access System')]

#Disconnect from clout portal
${DISCONNECT FORM}                    //form[@name='disconnectForm']
${DISCONNECT FORM HEADER}            //h1['Disconnect System from Nx Cloud?']

#Disconnect from my account
${DISCONNECT MODAL TEXT}              //p[contains(text(), 'You are about to disconnect this System from your account. You will not be able to access this System anymore. Are you sure?')]
${DISCONNECT MODAL CANCEL}            //button[@ng-click='cancel()']
${DISCONNECT MODAL DISCONNECT BUTTON}    //button[@ng-click='ok()']

${JUMBOTRON}                          //div[@class='jumbotron']
${PROMO BLOCK}                        //div[contains(@class,'promo-block') and not(contains(@class, 'col-sm-4'))]
${ALREADY ACTIVATED}                  //h1[@ng-if='!activate.success' and contains(text(),'Account is already activated or confirmation code is incorrect')]

#Share Elements (Note: Share and Permissions are the same form so these are the same variables.  Making two just in case they do diverge at some point.)
${SHARE MODAL}                        //div[@uib-modal-transclude]
${SHARE EMAIL}                        //form[@name='shareForm']//input[@ng-model='user.email']
${SHARE PERMISSIONS DROPDOWN}         //form[@name='shareForm']//select[@ng-model='user.role']
${SHARE BUTTON MODAL}                 //form[@name='shareForm']//button[@ng-click='checkForm()']
${SHARE CANCEL}                       //form[@name='shareForm']//button[@ng-click='close()']
${SHARE CLOSE}                        //div[@uib-modal-transclude]//div[@ng-if='settings.title']//button[@ng-click='close()']
${SHARE PERMISSIONS ADMINISTRATOR}    //form[@name='shareForm']//select[@ng-model='user.role']//option[@label='Administrator']
${SHARE PERMISSIONS ADVANCED VIEWER}  //form[@name='shareForm']//select[@ng-model='user.role']//option[@label='Advanced Viewer']
${SHARE PERMISSIONS VIEWER}           //form[@name='shareForm']//select[@ng-model='user.role']//option[@label='Viewer']
${SHARE PERMISSIONS LIVE VIEWER}      //form[@name='shareForm']//select[@ng-model='user.role']//option[@label='Live Viewer']
${SHARE PERMISSIONS CUSTOM}           //form[@name='shareForm']//select[@ng-model='user.role']//option[@label='Custom']
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
${ACCOUNT FIRST NAME}                 //form[@name='accountForm']//input[@ng-model='account.first_name']
${ACCOUNT LAST NAME}                  //form[@name='accountForm']//input[@ng-model='account.last_name']
${ACCOUNT SAVE}                       //form[@name='accountForm']//button[@ng-click='checkForm()']

#Already logged in modal
${LOGGED IN CONTINUE BUTTON}          //div[@uib-modal-transclude]//button[@ng-click='ok()']
${LOGGED IN LOG OUT BUTTON}           //div[@uib-modal-transclude]//button[@ng-click='cancel()']

${CONTINUE BUTTON}                    //div[@uib-modal-window='modal-window']//button[@ng-class='settings.buttonClass' and @ng-click='ok()']
${CONTINUE MODAL}                     //div[@uib-modal-window='modal-window']

${300CHARS}                           QWErtyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmyy
${255CHARS}                           QWErtyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopas

#Emails
${EMAIL VIEWER}                       noptixqa+viewer@gmail.com
${EMAIL ADV VIEWER}                   noptixqa+advviewer@gmail.com
${EMAIL LIVE VIEWER}                  noptixqa+liveviewer@gmail.com
${EMAIL OWNER}                        noptixqa+owner@gmail.com
${EMAIL NOT OWNER}                    noptixqa+notowner@gmail.com
${EMAIL ADMIN}                        noptixqa+admin@gmail.com
${UNREGISTERED EMAIL}                 noptixqa+unregistered@gmail.com
${EMAIL NOPERM}                       noptixqa+noperm@gmail.com
${BASE PASSWORD}                      qweasd123
${ALT PASSWORD}                       qweasd1234

#Related to Auto Tests system
${AUTO TESTS URL}                     b4939b35-5b98-492b-a092-27fe8efeef38
${AUTO TESTS}                         //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[contains(text(),'Auto Tests')]/..
${AUTO TESTS TITLE}                   //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[contains(text(),'Auto Tests')]
${AUTO TESTS USER}                    //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[contains(text(),'Auto Tests')]/following-sibling::span[contains(@class,'user-name') and contains(text(),'TestFirstName TestLastName')]
${AUTO TESTS OPEN NX}                 //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[contains(text(),'Auto Tests')]/..//button[@ng-click='checkForm()']

${SYSTEMS SEARCH INPUT}               //div[@ng-if='systems.length']
${SYSTEMS TILE}                       //div[@ng-repeat="system in systems | filter:searchSystems as filtered"]

#AUTOTESTS (with no space) is an offline system used for testing offline status on the systems page
${AUTOTESTS OFFLINE}                  //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[contains(text(),'Autotests')]/following-sibling::span[contains(text(), 'offline')]
${AUTOTESTS OFFLINE OPEN NX}          //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[contains(text(),'Autotests')]/..//button[@ng-click='checkForm()']

#ASCII
${ESCAPE}                             \\27
${ENTER}                              \\13
${TAB}                                \\9
${SPACEBAR}                           \\32