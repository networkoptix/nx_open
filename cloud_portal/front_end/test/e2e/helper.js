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

    this.get = function (opt_url) {
        var url = opt_url || '/';
        browser.get(url);
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

    this.forms = {
        login: {
            openLink: element(by.linkText('Login')),
            dialog: element(by.css('.modal-dialog')),
            emailInput: element(by.css('.modal-dialog')).element(by.model('auth.email')),
            passwordInput: element(by.css('.modal-dialog')).element(by.model('auth.password')),
            submitButton: element(by.css('.modal-dialog')).element(by.buttonText('Login'))
        },
        register: {
            firstNameInput: element(by.model('account.firstName')),
            lastNameInput: element(by.model('account.lastName')),
            emailInput: element(by.model('account.email')),
            passwordGroup: element(by.css('password-input')),
            passwordInput: element(by.css('password-input')).element(by.css('input[type=password]')),
            submitButton: element(by.css('[form=registerForm]')).element(by.buttonText('Register'))
        },
        account: {
            firstNameInput: element(by.model('account.first_name')),
            lastNameInput: element(by.model('account.last_name')),
            submitButton: element(by.css('[form=accountForm]')).element(by.buttonText('Save'))
        },
        share: {
            shareButton: element(by.partialButtonText('Share')),
            emailField: element(by.model('share.accountEmail')),
            roleField: element(by.model('share.accessRole')),
            submitButton: element(by.css('process-button')).element(by.buttonText('Share'))
        },
        logout: {
            navbar: element(by.css('header')).element(by.css('.navbar')),
            dropdownToggle: element(by.css('header')).element(by.css('.navbar')).element(by.css('a[uib-dropdown-toggle]')),
            dropdownMenu: element(by.css('header')).element(by.css('.navbar')).element(by.css('[uib-dropdown-menu]')),
            logoutLink: element(by.css('header')).element(by.css('.navbar')).element(by.linkText('Logout'))
        },
        restorePassEmail: {
            emailInput: element(by.model('data.email')),
            emailInputWrap: element(by.model('data.email')).element(by.xpath('../..')),
            submitButton: element(by.buttonText('Restore password'))
        },
        restorePassPassword: {
            passwordInput: element(by.model('data.newPassword')).element(by.css('input[type=password]')),
            submitButton: element(by.buttonText('Save password'))
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
    this.userEmail = 'noptixqa+1@gmail.com'; // valid existing email
    this.userEmail2 = 'noptixqa+2@gmail.com';
    this.userEmailWrong = 'nonexistingperson@gmail.com';

    this.userEmailOwner = 'noptixqa+owner@gmail.com';
    this.userEmailAdmin = 'noptixqa+admin@gmail.com';
    this.userEmailViewer = 'noptixqa+viewer@gmail.com';
    this.userEmailAdvViewer = 'noptixqa+advviewer@gmail.com';
    this.userEmailLiveViewer = 'noptixqa+liveviewer@gmail.com';
    this.userEmailNoPerm = 'noptixqa+noperm@gmail.com';

    this.roles = {
        admin: 'option[label=admin]',
        viewer: 'option[label=viewer]',
        advViewer: 'option[label~=advanced]',
        liveViewer: 'option[label~=live]'
    };

    this.userFirstName = 'TestFirstName';
    this.userLastName = 'TestLastName';

    this.userPassword = this.basePassword;
    this.userPasswordNew = 'qweasd123qwe';
    this.userPasswordWrong = 'qweqwe123';

    this.userNameCyrillic = 'Кенгшщзх';
    this.userNameSmile = '☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★';
    this.userNameHierog = '您都可以享受源源不絕的好禮及優惠';
    this.userPasswordSymb = '~!@#$%^&*()_:";\'{}[]+<>?,./qweasdzxc';
    this.userPasswordTm = 'qweasdzxc123®™';

    this.inputLong300 = 'qwertyuiopasdfghhkljzxcvbnmqwertyuiopasdfghhkl' +
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

    this.loginSuccessElement = element.all(by.css('.auth-visible')).first(); // some element on page, that is only visible when user is authenticated
    //this.loggedOutElement = element(by.css('.container.ng-scope')).all(by.css('.auth-hidden')).first(); // some element on page visible to not auth user
    this.htmlBody = element(by.css('body'));

    this.checkElementFocusedBy = function(element, attribute) {
        expect(element.getAttribute(attribute)).toEqual(browser.driver.switchTo().activeElement().getAttribute(attribute));
    };

    this.login = function(email, password) {
        var loginButton = element(by.linkText('Login'));

        h.get(h.urls.homepage);
        browser.sleep(500);

        loginButton.click();

        this.loginFromCurrPage(email, password);

        // Check that element that is visible only for authorized user is displayed on page
        return h.loginSuccessElement.isDisplayed().then( function (isDisplayed) {
            if (!isDisplayed) {
                return protractor.promise.rejected('Login failed');
            }
        });
    };

    this.loginFromCurrPage = function(email, password) {
        var usrEmail = email || this.userEmail;
        var usrPassword = password || this.userPassword;

        h.forms.login.emailInput.sendKeys(usrEmail);
        h.forms.login.passwordInput.sendKeys(usrPassword);
        h.forms.login.submitButton.click();
        browser.sleep(2000); // such a shame, but I can't solve it right now
    };

    this.logout = function() {

        expect(h.forms.logout.dropdownToggle.isPresent()).toBe(true);
        h.forms.logout.dropdownToggle.getText().then(function(text) {
            if(h.isSubstr(text, 'noptixqa')) {
                h.forms.logout.dropdownToggle.click();
                h.forms.logout.logoutLink.click();
                browser.sleep(500); // such a shame, but I can't solve it right now

                // Check that element that is visible only for authorized user is NOT displayed on page
                expect(h.loginSuccessElement.isDisplayed()).toBe(false);
            }
        });
    };

    this.fillRegisterForm = function(firstName, lastName, email, password) {
        var userFistName = firstName || this.userFirstName;
        var userLastName = lastName || this.userLastName;
        var userEmail = email || this.getRandomEmail();
        var userPassword = password || this.userPassword;

        h.forms.register.firstNameInput.sendKeys(userFistName);
        h.forms.register.lastNameInput.sendKeys(userLastName);
        h.forms.register.emailInput.sendKeys(userEmail);
        h.forms.register.passwordInput.sendKeys(userPassword);

        h.forms.register.submitButton.click();
    };

    this.register = function(firstName, lastName, email, password) {
        this.get(this.urls.register);
        expect(h.forms.register.firstNameInput.isPresent()).toBe(true);
        this.fillRegisterForm(firstName, lastName, email, password);
        expect(h.alert.successMessageElem.isDisplayed()).toBe(true);
        expect(h.alert.successMessageElem.getText()).toContain(this.alert.alertMessages.registerSuccess);
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

    this.getEmailedLink = function(userEmail, subject, where) {
        var deferred = protractor.promise.defer();

        browser.controlFlow().wait(this.getEmailTo(userEmail, subject).then(function (email) {
            deferred.fulfill(h.getUrlFromEmail(email, userEmail, subject, where));
        }));

        return deferred.promise;
    };

    this.createUser = function(firstName, lastName, email, password) {
        var deferred = protractor.promise.defer();
        var userEmail = email || h.getRandomEmail();

        h.register(firstName, lastName, userEmail, password);
        h.getEmailedLink(userEmail, h.emailSubjects.register, 'activate').then( function(url) {
            h.get(url);
            expect(h.htmlBody.getText()).toContain(h.alert.alertMessages.registerConfirmSuccess);
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

    this.shareSystemWith = function(email, role, systemLink) {
        var sharedRole = role || 'admin';
        var systemLinkCode = systemLink || '/a74840da-f135-4522-abd9-5a8c6fb8591f';
        var roleOption = h.forms.share.roleField.element(by.css(sharedRole));

        this.getSysPage(systemLinkCode);

        h.forms.share.shareButton.click();
        h.forms.share.emailField.sendKeys(email);
        h.forms.share.roleField.click();
        roleOption.click();
        h.forms.share.submitButton.click();
        expect(element(by.cssContainingText('td', email)).isPresent()).toBe(true);
    };

    this.emailSubjects = {
        register: "Confirm your account",
        restorePass: "Restore your password",
        share: ""
    };

    this.readPrevEmails = function() {
        var deferred = protractor.promise.defer();
        function onPrevMail(mail) {
            console.log("Open email to: " + mail.headers.to);
            deferred.fulfill(mail);
            notifier.stop();
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

    this.forms.restorePassEmail.sendLinkToEmail = function(userEmail) {
        h.get(h.urls.restore_password);
        h.forms.restorePassEmail.emailInput.clear().sendKeys(userEmail);
        h.forms.restorePassEmail.submitButton.click();
    };

    this.forms.restorePassPassword.setNewPassword = function(newPassword) {
        var here = h.forms.restorePassPassword;
        expect(here.passwordInput.isPresent()).toBe(true);
        here.passwordInput.sendKeys(newPassword);
        here.submitButton.click();
        expect(h.alert.successMessageElem.isDisplayed()).toBe(true);
        expect(h.alert.successMessageElem.getText()).toContain(h.alert.alertMessages.restorePassSuccess);
    };

    this.waitIfNotPresent = function(element, timeout) {
        var timeoutUsed = timeout || 1000;
        element.isPresent().then( function(isPresent) {
            if(!isPresent) {
                browser.sleep(timeoutUsed);
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