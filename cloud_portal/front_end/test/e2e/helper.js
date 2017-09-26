'use strict';

/*
    Tests cheat-sheet:

    Use x to disable suit or case, e.g.
    xdescribe - to disable all specs in the suit
    xit - to disable one spec

    Use f to run only one suit or case, e.g.
    fdescribe  - to run only this suit
    fit - to run only this spec
 */

var Helper = function () {
    var AlertSuite = require('./alerts_check.js');
    this.alert = new AlertSuite();

    var h = this;

    this.basePassword = 'qweasd123';
    this.systemLink = '/f52c704d-0398-475e-bfc2-ccd88c44e20e';
    this.systemName = 'autotest';

    this.get = function (opt_url) {
        var url = opt_url || '/';
        browser.get(url);
        browser.waitForAngular();
    };

    this.refresh = function () {
        browser.refresh();
        browser.waitForAngular();
    };

    this.urls = {
        homepage: '/',
        account: '/account',
        password_change: '/account/password',
        systems: '/systems',
        register: '/register',
        restore_password: '/restore_password'
    };
    this.navigateBack = function (){
        browser.navigate().back();
        browser.waitForAngular();
    };

    this.getSysPage = function(systemLink) {
        this.get(this.urls.systems + systemLink);
    };

    this.getParentOf = function(field) {
        return field.element(by.xpath('..'));
    };

    this.getGrandParentOf = function (field) {
      return h.getParentOf(h.getParentOf(field));
    };

    this.forms = {
        login: {
            openLink: element(by.css('a[href="/login"]')),
            dialog: element(by.css('.modal-dialog')),
            emailInput: element(by.css('.modal-dialog')).element(by.model('auth.email')),
            passwordInput: element(by.css('.modal-dialog')).element(by.model('auth.password')),
            submitButton: element(by.css('.modal-dialog')).element(by.css('process-button[process="login"]')),
            forgotPasswordLink: element(by.css('.modal-dialog')).element(by.linkText('Forgot password?')),
            messageLoginLink: element(by.css('h1')).element(by.css('a[href="/login"]'))
        },
        register: {
            triggerRegisterButton: element(by.linkText('Create Account')),
            firstNameInput: element(by.model('account.firstName')),
            lastNameInput: element(by.model('account.lastName')),
            emailInput: element(by.model('account.email')),
            passwordGroup: element(by.css('password-input')),
            passwordInput: element(by.css('password-input')).element(by.css('input[type=password]')),
            submitButton: element(by.css('[form=registerForm]')).element(by.buttonText('Create Account')),
            processSuccess: element(by.css('.process-success[ng-if="activate.success"]'))
        },
        account: {
            firstNameInput: element(by.model('account.first_name')),
            lastNameInput: element(by.model('account.last_name')),
            submitButton: element(by.css('[form=accountForm]')).element(by.buttonText('Save'))
        },
        password: {
            currentPasswordInput: element(by.model('pass.password')),
            passwordInput: element(by.model('pass.newPassword')).element(by.css('input[type=password]')),
            submitButton: element(by.css('[form=passwordForm]')).element(by.buttonText('Change Password'))
        },
        share: {
            shareButton: element(by.css('button[ng-click="share()"]')),
            emailField: element(by.model('user.email')),
            roleField: element(by.model('user.role')),
            submitButton: element(by.css('process-button[process="sharing"]'))
        },
        logout: {
            navbar: element(by.css('header')).element(by.css('.navbar')),
            dropdownToggle: h.getParentOf(element(by.css('span.glyphicon-user'))),
            dropdownMenu: h.getGrandParentOf(element(by.css('span.glyphicon-user'))).element(by.css('[uib-dropdown-menu]')),
	    dropdownParent: h.getGrandParentOf(element(by.css('span.glyphicon-user'))),
            //dropdownMenu: element(by.css('header')).element(by.css('.navbar')).element(by.css('[uib-dropdown-menu]')),
            logoutLink: element(by.css('header')).element(by.css('.navbar')).all(by.css('a[ng-click="logout()"]')).first(),
            alreadyLoggedIn: element(by.css('.authorized.modal-open')),
            logOut: element(by.css('button[ng-click="cancel()"]'))
        },
        restorePassEmail: {
            emailInput: element(by.model('data.email')),
            emailInputWrap: element(by.model('data.email')).element(by.xpath('../..')),
            submitButton: element(by.buttonText('Reset Password'))
        },
        restorePassPassword: {
            passwordInput: element(by.model('data.newPassword')).element(by.css('input[type=password]')),
            submitButton: element(by.buttonText('Save password')),
            messageLoginLink: element(by.css('h1')).element(by.linkText('Log In'))
        }
    };

    // Get valid email with random number between 10000 and 100000
    this.getRandomEmail = function() {
        var randomNumber = Math.floor((Math.random() * 100000)+10000); // Random number between 1000 and 10000
        return 'noptixqa+' + randomNumber + '@gmail.com';
    };
    // Get valid email with random number between 10000 and 100000
    this.getRandomEmailWith = function(addition) {
        var randomNumber = Math.floor((Math.random() * 100000)+10000); // Random number between 1000 and 10000
        var email = 'noptixqa+'+ addition + randomNumber + '@gmail.com';
        console.log(email);
        return email;
    };
    //this.userEmail = 'noptixqa+owner@gmail.com';
    this.userEmailSubstring = 'noptixqa'; // this will be used to check if any user with chosen mailbox is logged in
    this.userEmail = 'noptixqa+1@gmail.com'; // valid existing email
    this.userEmail2 = 'noptixqa+2@gmail.com';
    this.userEmailWrong = 'nonexistingperson@gmail.com';

    this.userEmailOwner = 'noptixqa+owner@gmail.com';
    this.userEmailAdmin = 'noptixqa+admin@gmail.com';
    this.userEmailViewer = 'noptixqa+viewer@gmail.com';
    this.userEmailAdvViewer = 'noptixqa+advviewer@gmail.com';
    this.userEmailLiveViewer = 'noptixqa+liveviewer@gmail.com';
    this.userEmailCustom = 'noptixqa+custom@gmail.com';
    this.userEmailSmoke = 'noptixqa+smoke@gmail.com';
    this.userEmailNoPerm = 'noptixqa+noperm@gmail.com';

    this.roles = {
        owner: 'Owner',
        admin: 'Administrator',
        viewer: 'Viewer',
        advViewer: 'Advanced Viewer',
        liveViewer: 'Live Viewer',
        custom: 'Custom'
    };

    this.userFirstName = 'TestFirstName';
    this.userLastName = 'TestLastName';

    this.userPassword = h.basePassword;
    this.userPasswordNew = 'qweasd123qwe';
    this.userPasswordWrong = 'qweqwe123';

    this.userNameCyrillic = 'Кенгшщзх';
    this.userNameSmile = '☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★';
    this.userNameHierog = '您都可以享受源源不絕的好禮及優惠';
    this.userPasswordSymb = '~!@#$%^&*()_:";\'{}[]+<>?,./qweasdzxc';
    this.userPasswordTm = 'qweasdzxc123®™';

    this.inputLong300 = 'QWErtyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkl' +
        'jzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnm' +
        'qwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyui' +
        'opasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfgh' +
        'hkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkljzxcvbnmyy';
    this.inputLongCut = this.inputLong300.substr(0, 255);

    this.inputSymb = '|`~-=!@#$%^&*()_:\";\'{}[]+<>?,./';

    this.userPasswordCyrillic = 'йцуфывячс';
    this.userPasswordSmile = '☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★';
    this.userPasswordHierog = '您都可以享受源源不絕的好禮及優惠';
    this.userPasswordWrong = 'qweqwe123';

    this.loginSuccessElement = element(by.css('span.glyphicon-user')); // some element on page, that is only visible when user is authenticated
    this.loginNoSysSuccessElement = element(by.cssContainingText('span','You have no Systems connected to')); // some element on page, that is only visible when user is authenticated
    this.loginSysPageSuccessElement = element.all(by.css('[ng-if="gettingSystem.success"]')).get(0); // some element on page, that is only visible when user is authenticated
    this.loginSysPageClosedSuccessElement = element(by.cssContainingText('.no-data-panel-body','You do not have access to the system information.')); // some element on page, that is only visible when user is authenticated
    this.htmlBody = element(by.css('body'));

    this.systemsList = this.getParentOf(element(by.cssContainingText('h1', 'Systems'))).element(by.css('div.row'));
    this.allSystems = this.systemsList.all(by.css('div.system-button'));

    this.systemsWithCurrentName = this.allSystems.filter(function (system) {
        return system.getText().then(function (text) {
            // Check that system tile contains declared system name inside
            return h.isSubstr(text, h.systemName);
        });
    });

    this.onlineSystemsWithCurrentName = this.systemsWithCurrentName.filter(function (system) {
        return system.element(by.cssContainingText('button', 'Open in ')).isPresent().then(function (isPresent) {
            return isPresent
        });
    });

    this.getSystemsWithName = function (name) {
        var name = name || h.systemName;

        return h.allSystems.filter(function (system) {
            return system.getText().then(function (text) {
                // Check that system tile contains declared system name inside
                return h.isSubstr(text, name);
            });
        });
    };

    this.getOnlineSystemsWithName = function (name) {
        var name = name || h.systemName;

        return h.getSystemsWithName().filter(function (system) {
            return system.element(by.cssContainingText('button', 'Open in ')).isPresent().then(function (isPresent) {
                return isPresent
            });
        });
    };

    this.checkElementFocusedBy = function(element, attribute) {
        expect(element.getAttribute(attribute)).toEqual(browser.driver.switchTo().activeElement().getAttribute(attribute));
    };

    this.login = function(email, password) {
        var loginButton = h.forms.login.openLink;

        h.get(h.urls.homepage);
        browser.sleep(500);
        loginButton.click();

        this.loginFromCurrPage(email, password);

        // Check that element that is visible only for authorized user is displayed on page
        return h.loginSuccessElement.isPresent().then( function (isPresent) {
            if (!isPresent) {
                return protractor.promise.rejected('Login failed');
            }
        });
    };

    this.loginToSystems = function(email, password) {
        this.login(email, password);
        browser.get('/systems');
    };

    this.loginFromCurrPage = function(email, password) {
        var usrEmail = email || this.userEmailOwner;
        var usrPassword = password || this.userPassword;

        h.forms.login.emailInput.sendKeys(usrEmail);
        h.forms.login.passwordInput.sendKeys(usrPassword);
        h.forms.login.submitButton.click();
        browser.sleep(2000);
    };

    this.logout = function() {

        h.loginSuccessElement.isDisplayed().then(function(isDisplayed) {
            if(isDisplayed) {
                browser.sleep(1000);
                h.forms.logout.dropdownToggle.click().then(function () {
                    h.forms.logout.logoutLink.click();
                });
                browser.sleep(1500);

                expect(h.loginSuccessElement.isDisplayed()).toBe(false);
            }
            else {
                console.log('FAILED TO LOG OUT: user is already logged out');
            }
        });
    };

    this.sleepInIgnoredSync = function(timeout) {
        browser.ignoreSynchronization = true;
        browser.sleep(timeout);
        browser.ignoreSynchronization = false;
    };

    this.doInIgnoredSync = function(callback) {
        browser.ignoreSynchronization = true;
        callback();
        browser.ignoreSynchronization = false;
    };

    this.fillRegisterForm = function(firstName, lastName, email, password) {
        var userFistName = firstName || this.userFirstName;
        var userLastName = lastName || this.userLastName;
        var userEmail = email || this.getRandomEmail();
        var userPassword = password || this.userPassword;

        // Log out if logged in
        h.checkPresent(h.forms.logout.alreadyLoggedIn).then( function () {
            h.forms.logout.logOut.click();
            browser.sleep(3000);
        }, function () {});

        h.forms.register.firstNameInput.sendKeys(userFistName);
        h.forms.register.lastNameInput.sendKeys(userLastName);
        h.forms.register.emailInput.sendKeys(userEmail);
        h.forms.register.passwordInput.sendKeys(userPassword);

        h.forms.register.submitButton.click();
    };

    this.fillRegisterFormWithEmail = function(firstName, lastName, password) {
        var userFistName = firstName || this.userFirstName;
        var userLastName = lastName || this.userLastName;
        var userPassword = password || this.userPassword;

        h.sleepInIgnoredSync(2000);

        // Log out if logged in
        h.checkPresent(h.forms.logout.alreadyLoggedIn).then( function () {
            h.forms.logout.logOut.click();
        }, function () {});

        // h.sleepInIgnoredSync(2000);
        browser.sleep(2000);

        h.forms.register.firstNameInput.sendKeys(userFistName);
        h.forms.register.lastNameInput.sendKeys(userLastName);
        h.forms.register.passwordInput.sendKeys(userPassword);

        h.forms.register.submitButton.click();
    };

    this.register = function(firstName, lastName, email, password) {
        var deferred = protractor.promise.defer();
        var alreadyRegisteredText = 'This Email has been already registered in';
        var alreadyRegisteredElement = element(by.cssContainingText('.help-block', alreadyRegisteredText));

        h.get(h.urls.register);

        h.fillRegisterForm(firstName, lastName, email, password);
        h.alert.successMessageElem.isPresent().then(function (isPresent) {
            if(isPresent) {
                expect(h.alert.successMessageElem.getText()).toContain(h.alert.alertMessages.registerSuccess);
                deferred.fulfill();
            }
            else {
                alreadyRegisteredElement.isDisplayed().then( function (isDisplayed) {
                    if (isDisplayed) {
                        deferred.reject('Registration failed: user is already registered. Continue without failing test.');
                    }
                    else deferred.reject('Registration failed, or wrong success message is shown');
                });
            }
        });

        return deferred.promise;
    };

    this.registerAnyLang = function(firstName, lastName, email, password) {
        var deferred = protractor.promise.defer();
        var alreadyRegisteredElement = element(by.css('.help-block[ng-if="registerForm.registerEmail.$error.alreadyExists"]'));

        h.get(h.urls.register);

        h.fillRegisterForm(firstName, lastName, email, password);
        h.alert.successMessageElem.isPresent().then(function (isPresent) {
            if(isPresent) {
                expect(h.alert.successMessageElem.getText()).toContain(email);
                deferred.fulfill();
            }
            else {
                alreadyRegisteredElement.isDisplayed().then( function (isDisplayed) {
                    if (isDisplayed) {
                        deferred.reject('Registration failed: user is already registered. Continue without failing test.');
                    }
                    else deferred.reject('Registration failed, or wrong success message is shown');
                });
            }
        });

        return deferred.promise;
    };

    this.getUrlFromEmail = function(email, userEmail, subject, where) {
        expect(email.subject).toContain(subject);
        expect(email.headers.to).toEqual(userEmail);

        // extract registration token from the link in the email message
        var pattern = new RegExp(where + '/([\\w=]+)', 'g');
        var regCode = pattern.exec(email.html)[1];
        console.log(regCode);
        return ('/' + where + '/' + regCode); // url
    };

    this.getUrlFromEmailAndCheckPortal = function(email, portalText, userEmail, subject, where) {
        expect(email.subject).toContain(subject);
        expect(email.subject).toContain(portalText);
        expect(email.headers.to).toEqual(userEmail);

        // extract registration token from the link in the email message
        var pattern = new RegExp(where + '/([\\w=]+)', 'g');
        var regCode = pattern.exec(email.html)[1];
        console.log(regCode);
        return ('/' + where + '/' + regCode); // url
    };

    this.getUrlFromEmailAndCheckPortalAnyLang = function(email, portalText, userEmail, where) {
        expect(email.subject).toContain(portalText);
        expect(email.headers.to).toEqual(userEmail);

        // extract registration token from the link in the email message
        var pattern = new RegExp(where + '/([\\w=]+)', 'g');
        var regCode = pattern.exec(email.html)[1];
        console.log(regCode);
        return ('/' + where + '/' + regCode); // url
    };

    this.getEmailedLink = function(userEmail, subject, where) {
        var deferred = protractor.promise.defer();

        browser.controlFlow().wait(this.getEmailTo(userEmail, subject).then(function (email) {
            deferred.fulfill(h.getUrlFromEmail(email, userEmail, subject, where));
        }));

        return deferred.promise;
    };

    this.getEmailedLinkCustomAnyLang = function(userEmail, portalText, where) {
        var deferred = protractor.promise.defer();

        browser.controlFlow().wait(this.getEmailToAnyLang(userEmail, where).then(function (email) {
            deferred.fulfill(h.getUrlFromEmailAndCheckPortalAnyLang(email, portalText, userEmail, where));
        }));

        return deferred.promise;
    };

    this.getEmailedLinkCustom = function(userEmail, portalText, subject, where) {
        var deferred = protractor.promise.defer();

        browser.controlFlow().wait(this.getEmailTo(userEmail, subject).then(function (email) {
            deferred.fulfill(h.getUrlFromEmailAndCheckPortal(email, portalText, userEmail, subject, where));
        }));

        return deferred.promise;
    };

    this.createUser = function(firstName, lastName, email, password) {
        var deferred = protractor.promise.defer();
        var userEmail = email || h.getRandomEmail();

        h.register(firstName, lastName, userEmail, password).then(function () {
            h.getEmailedLink(userEmail, h.emailSubjects.register, 'activate').then( function(url) {
                h.get(url);
                expect(h.htmlBody.getText()).toContain(h.alert.alertMessages.registerConfirmSuccess);
                deferred.fulfill(userEmail);
            });
        }, function(errorMessage) { // on reject
            console.log(errorMessage);
            deferred.fulfill(userEmail);
        });

        return deferred.promise;
    };

    this.createUserIfMissing = function(firstName, lastName, email, password) {
        var deferred = protractor.promise.defer();
        var userEmail = email;

        h.register(firstName, lastName, userEmail, password).then(function () {
            h.getEmailedLink(userEmail, h.emailSubjects.register, 'activate').then( function(url) {
                h.get(url);
                deferred.fulfill(userEmail);
            });
        }, function(errorMessage) { // on reject
            console.log(errorMessage);
            deferred.fulfill(userEmail);
        });

        return deferred.promise;
    };

    this.createUserCustom = function(portalText, firstName, lastName, email, password) {
        var deferred = protractor.promise.defer();
        var userEmail = email || h.getRandomEmail();

        h.register(firstName, lastName, userEmail, password).then(function () {
            h.getEmailedLinkCustom(userEmail, portalText, h.emailSubjects.register, 'activate').then( function(url) {
                h.get(url);
                expect(h.htmlBody.getText()).toContain(h.alert.alertMessages.registerConfirmSuccess);
                deferred.fulfill(userEmail);
            });
        }, function(errorMessage) { // on reject
            console.log(errorMessage);
            deferred.fulfill(userEmail);
        });

        return deferred.promise;
    };

    this.createUserAnyLang = function(portalText, firstName, lastName, email, password) {
        var deferred = protractor.promise.defer();
        var userEmail = email || h.getRandomEmail();

        h.registerAnyLang(firstName, lastName, userEmail, password).then(function () {
            h.getEmailedLinkCustomAnyLang(userEmail, portalText, 'activate').then( function(url) {
                h.get(url);
                expect((h.forms.register.processSuccess).isPresent()).toBe(true);
                deferred.fulfill(userEmail);
            });
        }, function(errorMessage) { // on reject
            console.log(errorMessage);
            deferred.fulfill(userEmail);
        });

        return deferred.promise;
    };


    this.changeAccountNames = function(firstName, lastName) {
        h.get(h.urls.account);
        h.forms.account.firstNameInput.clear().sendKeys(firstName);
        h.forms.account.lastNameInput.clear().sendKeys(lastName);
        h.forms.account.submitButton.click();

        h.alert.catchAlert( h.alert.alertMessages.accountSuccess, h.alert.alertTypes.success);
    };

    this.openSystemByLink = function (systemLink) {
        var systemLinkCode = systemLink || h.systemLink;

        this.getSysPage(systemLinkCode);
        browser.waitForAngular();
    };

    // Opens first online system with specified name.
    this.openSystemFromSystemList = function(name) {
        var name = name || h.systemName;

        // In case any other page (except systems list or system page) is opened, open system list
        h.systemsList.isPresent().then(function (isPresent) {
            if(!isPresent) {
                h.get(h.urls.systems); // if user is not redirected to system list, open it manually
            }
        });

        // If system is the only one and it is already opened, do nothing.
        // Otherwise, open first online system with specified name
        element(by.cssContainingText('h1', name)).isPresent().then(function (isPresent) {
            if(!isPresent) {
                expect(h.getOnlineSystemsWithName(name).first().isDisplayed()).toBe(true);
                h.onlineSystemsWithCurrentName.first().click();
            }
        });
    };

    this.shareSystemWith = function(email, role) {
        var sharedRole = role || 'Administrator';
        var roleOption = h.forms.share.roleField.element(by.cssContainingText('option', sharedRole));

        h.forms.share.shareButton.click();
        h.forms.share.emailField.sendKeys(email);
        h.forms.share.roleField.click();
        roleOption.click();
        browser.waitForAngular();

        h.forms.share.submitButton.click();
        expect(element(by.cssContainingText('td', email)).isPresent()).toBe(true);
    };

    this.registerByInvite = function (firstName, lastName, email, password) {
        // var deferred = protractor.promise.defer();
        var userEmail = email || h.getRandomEmail();
        var password = password || h.password;

        h.openSystemByLink();
        h.shareSystemWith(userEmail);
        h.getEmailedLink(userEmail, h.emailSubjects.invite, 'register').then( function(url) {
            h.get(url);
        });

        h.fillRegisterFormWithEmail(firstName, lastName, password);
        browser.sleep(1000);

        // h.alert.successMessageElem.isPresent().then(function (isPresent) {
        //     browser.pause();
        //
        //     if(isPresent) {
        //         expect(h.alert.successMessageElem.getText()).toContain(h.alert.alertMessages.registerSuccess);
        //         deferred.fulfill();
        //     }
        //     else {
        //         deferred.reject('Registration failed, or wrong success message is shown');
        //     }
        // });

        // return deferred.promise;
    };

    this.registerByInviteAllLang = function (portalText, firstName, lastName, email, password) {
        var userEmail = email || h.getRandomEmail();
        var password = password || h.password;

        h.openSystemByLink();
        h.shareSystemWith(userEmail);
        h.getEmailedLinkCustomAnyLang(userEmail, portalText , 'register').then( function(url) {
            h.get(url);
            h.fillRegisterFormWithEmail(firstName, lastName, password);
            browser.sleep(1000);
        });
    };

    this.emailSubjects = {
        register: "Confirm your account",
        invite: " was shared with you",
        restorePass: "Restore your password",
        share: " was shared with you"
    };

    this.readPrevEmails = function() {
        var deferred = protractor.promise.defer();
        function onPrevMail(mail) {
            console.log("Open email to: " + mail.headers.to);
            deferred.fulfill(mail);
            // Commented out because it was causing "ReferenceError: self is not defined"
            //notifier.stop();
            notifier.removeListener("mail", onPrevMail);
        }
        notifier.on("mail", onPrevMail);
        notifier.start();
        return deferred.promise;
    };

    this.getEmailTo = function(emailAddress, emailSubject) {
        var deferred = protractor.promise.defer();
        console.log("Waiting for an email...");

        function onMail(mail) {
            if((emailAddress === mail.headers.to) && (mail.subject.includes(emailSubject))) {
                console.log("Catch email to: " + mail.headers.to);
                deferred.fulfill(mail);
                //notifier.stop();
                // commented out because causes  "ReferenceError: self is not defined" at Notifier.stop
                // (/home/ekorneeva/develop/nx_vms/cloud_portal/front_end/node_modules/mail-notifier/index.js:106:5)
                notifier.removeListener("mail", onMail);
                return;
            }
            console.log("Ignore email to: " + mail.headers.to);
        }

        notifier.on("mail", onMail);

        notifier.start();

        return deferred.promise;
    };

    this.getEmailToAnyLang = function(emailAddress, emailSubject) {
        var deferred = protractor.promise.defer();
        console.log("Waiting for an email...");

        function onMail(mail) {
            if((emailAddress === mail.headers.to) && (mail.html.includes(emailSubject))) {
                console.log("Catch email to: " + mail.headers.to);
                deferred.fulfill(mail);
                notifier.stop();
                notifier.removeListener("mail", onMail);
                return;
            }
            console.log("Ignore email to: " + mail.headers.to);
        }

        notifier.on("mail", onMail);

        notifier.start();

        return deferred.promise;
    };

    this.restorePassword = function(userEmail, newPassword) {
        var password = newPassword || h.userPassword;
        var email = userEmail || h.userEmail;
        h.forms.restorePassEmail.sendLinkToEmail(email);
        expect(h.alert.successMessageElem.isDisplayed()).toBe(true);
        expect(h.alert.successMessageElem.getText()).toContain(h.alert.alertMessages.restorePassConfirmSent);

        h.getEmailedLink(email, h.emailSubjects.restorePass, 'restore_password').then(function(url) {
            h.get(url);
            h.forms.restorePassPassword.setNewPassword(password);
        });
    };

    this.restorePasswordAnyLang = function(portalText, userEmail, newPassword) {
        var deferred = protractor.promise.defer();

        var password = newPassword || h.userPassword;
        var email = userEmail || h.userEmail;
        h.forms.restorePassEmail.sendLinkToEmail(email);
        expect(h.alert.successMessageElem.isDisplayed()).toBe(true);
        expect(h.alert.successMessageElem.getText()).toContain(email);

        h.getEmailedLinkCustomAnyLang(email, portalText, 'restore_password').then(function(url) {
            h.get(url);
            h.forms.restorePassPassword.setNewPasswordAnyLang(password);
            deferred.fulfill(userEmail);
        });
        return deferred.promise;
    };

    this.forms.restorePassEmail.sendLinkToEmail = function(userEmail) {
        h.get(h.urls.restore_password);
        h.forms.restorePassEmail.emailInput.clear().sendKeys(userEmail);
        h.forms.restorePassEmail.submitButton.click();
    };

    this.forms.restorePassPassword.setNewPassword = function(newPassword) {
        var here = h.forms.restorePassPassword;

        // Log out if logged in
        h.checkPresent(h.forms.logout.alreadyLoggedIn).then( function () {
            h.forms.logout.logOut.click();
            browser.sleep(3000);
        }, function () {});

        expect(here.passwordInput.isPresent()).toBe(true);
        here.passwordInput.sendKeys(newPassword);
        here.submitButton.click();
        browser.sleep(1000);
        // expect(h.alert.successMessageElem.isDisplayed()).toBe(true);
        expect(h.alert.successMessageElem.getText()).toContain(h.alert.alertMessages.restorePassSuccess);
    };

    this.forms.restorePassPassword.setNewPasswordAnyLang = function(newPassword) {
        var here = h.forms.restorePassPassword;

        // Log out if logged in
        h.checkPresent(h.forms.logout.alreadyLoggedIn).then( function () {
            h.forms.logout.logOut.click()
        }, function () {});

        expect(here.passwordInput.isPresent()).toBe(true);
        here.passwordInput.sendKeys(newPassword);
        here.submitButton.click();
        expect(h.alert.successMessageElem.isDisplayed()).toBe(true);
    };

    this.checkPresent = function(elem) {
        var deferred = protractor.promise.defer();
        elem.isPresent().then( function(isPresent) {
            if(isPresent)   deferred.fulfill();
            else            deferred.reject("Element was not found, but that's expected.");
        });

        return deferred.promise;
    };

    this.waitIfNotPresent = function(element, checkInterval) {
        var checkInterval = checkInterval || 1000;
        element.isPresent().then( function(isPresent) {
            if(!isPresent) {
                browser.sleep(checkInterval);
                h.waitIfNotPresent(element, checkInterval);
            }
        })
    };

    this.isSubstr = function(string, substring) {
        if (string.indexOf(substring) > -1) return true;
    };

    var fs = require('fs');

    // writing screen shot to a file
    this.writeScreenShot = function(data, filename) {
        var stream = fs.createWriteStream(filename);

        stream.write(new Buffer(data, 'base64'));
        stream.end();
    }

};

module.exports = Helper;
