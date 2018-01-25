*** Variables ***
${ALERT}                              //div[contains(@class, 'ng-toast')]//span[@ng-bind-html='message.content']

${LOCAL}                              https://localhost:9000/

${CLOUD TEST}                         https://cloud-test.hdw.mx
${CLOUD TEST REGISTER}                https://cloud-test.hdw.mx/register

#Log In Elements
${LOG IN MODAL}                       //div[contains(@class, 'modal-content')]
${EMAIL INPUT}                        //form[contains(@name, 'loginForm')]//input[@ng-model='auth.email']
${PASSWORD INPUT}                     //form[contains(@name, 'loginForm')]//input[@ng-model='auth.password']
${LOG IN BUTTON}                      //form[contains(@name, 'loginForm')]//button[@ng-click='checkForm()']
${REMEMBER ME CHECKBOX}               //form[contains(@name, 'loginForm')]//input[@ng-model='auth.remember']
${FORGOT PASSWORD}                    //form[contains(@name, 'loginForm')]//a[@href='/restore_password']

${RESET PASSWORD EMAIL INPUT}         //form[@name='restorePassword']//input[@ng-model='data.email']

${LOG IN NAV BAR}                     //nav//a[contains(@ng-click, 'login()')]

${ACCOUNT DROPDOWN}                   //li[contains(@class, 'collapse-first')]//a['uib-dropdown-toggle']
${LOG OUT BUTTON}                     //li[contains(@class, 'collapse-first')]//a[contains(text(), 'Log Out')]

#Register Form Elements
${FIRST NAME INPUT}                   //form[@name= 'registerForm']//input[@ng-model='account.firstName']
${LAST NAME INPUT}                    //form[@name= 'registerForm']//input[@ng-model='account.lastName']
${REGISTER EMAIL INPUT}               //form[@name= 'registerForm']//input[@ng-model='account.email']
${REGISTER PASSWORD INPUT}            //form[@name= 'registerForm']//password-input[@ng-model='account.password']//input[@type='password']
${CREATE ACCOUNT BUTTON}              //form[@name= 'registerForm']//button[@ng-bind-html='buttonText' and contains(text(), 'Create Account')]
${TERMS AND CONDITIONS LINK}          //form[@name= 'registerForm']//a[@href='/content/eula' and contains(text(), 'Terms and Conditions')]
${RESEND ACTIVATION LINK BUTTON}      //form[@name= 'reactivateAccount']//button[contains(text(), 'Resend activation link')]

${ACTIVATION SUCCESS}                 //div[@ng-model-options="{ updateOn: 'blur' }"]//h1[@ng-if='activate.success' and contains(text(), 'Your account is successfully activated')]
${SUCCESS LOG IN BUTTON}              //div[@ng-model-options="{ updateOn: 'blur' }"]//h1[@ng-if='activate.success' and contains(text(), 'Your account is successfully activated')]//a[@href='/login']

${SHARE BUTTON SYSTEMS}               //div[@process-loading='gettingSystem']//button[@ng-click='share()']
${DELETE USER MODAL}                  //div[@uib-modal-transclude]
${DELETE USER BUTTON}                 //button[@ng-click='ok()' and contains(text(), 'Delete')]
${DELETE USER CANCEL BUTTON}          //button[@ng-click='cancel()' and contains(text(), 'Cancel')]

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

${CONTINUE BUTTON}                    //div[@uib-modal-window='modal-window']//button[@ng-class='settings.buttonClass' and @ng-click='ok()']
${CONTINUE MODAL}                     //div[@uib-modal-window='modal-window']

${300CHARS}                           QWErtyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmyy
${255CHARS}                           QWErtyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopas

${EMAIL VIEWER}                       noptixqa+viewer@gmail.com
${EMAIL OWNER}                        noptixqa+owner@gmail.com
${UNREGISTERED EMAIL}                 noptixqa+unregistered@gmail.com
${BASE PASSWORD}                      qweasd123

${AUTO TESTS}                         //div[@ng-repeat='system in systems | filter:searchSystems as filtered']//h2[contains(text(),'Auto Tests')]/..

#ASCII
${ESCAPE}                             \\27
${ENTER}                              \\13
${TAB}                                \\9
${SPACEBAR}                           \\32