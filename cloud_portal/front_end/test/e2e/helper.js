'use strict';

var Helper = function () {
    var AlertSuite = require('./alerts_check.js');
    this.alert = new AlertSuite();

    var self = this;

    this.basePassword = 'qweasd123';

    this.get = function (url) {
        browser.get(url);
        browser.waitForAngular();
    };

    this.navigateBack = function (){
        browser.navigate().back();
        browser.waitForAngular();
    };

    // Get valid email with random number between 100 and 1000
    this.getRandomEmail = function() {
        var randomNumber = Math.floor((Math.random() * 10000)+1000); // Random number between 1000 and 10000
        return 'noptixqa+' + randomNumber + '@gmail.com';
    };
    // Get valid email with random number between 100 and 1000
    this.getRandomEmailWith = function(addition) {
        var randomNumber = Math.floor((Math.random() * 10000)+1000); // Random number between 1000 and 10000
        return 'noptixqa+'+ addition + randomNumber + '@gmail.com';
    };
    this.userEmail = 'noptixqa+1@gmail.com'; // valid existing email
    this.userEmail2 = 'noptixqa+2@gmail.com';
    this.userEmailWrong = 'nonexistingperson@gmail.com';

    this.userFirstName = 'TestFirstName';
    this.userLastName = 'TestLastName';

    this.userPassword = this.basePassword;
    this.userPasswordNew = 'qweasd123qwe';
    this.userPasswordWrong = 'qweqwe123';

    this.userNameCyrillic = 'Кенгшщзх';
    this.userNameSmile = '☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★';
    this.userNameHierog = '您都可以享受源源不絕的好禮及優惠';

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
        var loginDialog = element(by.css('.modal-dialog'));
        var emailInput = loginDialog.element(by.model('auth.email'));
        var passwordInput = loginDialog.element(by.model('auth.password'));
        var dialogLoginButton = loginDialog.element(by.buttonText('Login'));

        browser.get('/');
        browser.waitForAngular();
        browser.sleep(500);

        loginButton.click();

        emailInput.sendKeys(email);
        passwordInput.sendKeys(password);

        dialogLoginButton.click();
        browser.sleep(2000); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is displayed on page
        expect(this.loginSuccessElement.isDisplayed()).toBe(true);
    };

    this.logout = function() {
        var navbar = element(by.css('header')).element(by.css('.navbar'));
        var userAccountDropdownToggle = navbar.element(by.css('a[uib-dropdown-toggle]'));
        var userAccountDropdownMenu = navbar.element(by.css('[uib-dropdown-menu]'));
        var logoutLink = userAccountDropdownMenu.element(by.linkText('Logout'));

        expect(userAccountDropdownToggle.isDisplayed()).toBe(true);

        userAccountDropdownToggle.click();
        logoutLink.click();
        browser.sleep(500); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(this.loginSuccessElement.isDisplayed()).toBe(false);
    };

    this.register = function() {
        var firstNameInput = element(by.model('account.firstName'));
        var lastNameInput = element(by.model('account.lastName'));
        var emailInput = element(by.model('account.email'));
        var passwordGroup = element(by.css('password-input'));
        var passwordInput = passwordGroup.element(by.css('input[type=password]'));
        var submitButton = element(by.css('[form=registerForm]')).element(by.buttonText('Register'));

        var userEmail = this.getRandomEmail();

        browser.get('/#/register');

        firstNameInput.sendKeys(this.userFirstName);
        lastNameInput.sendKeys(this.userLastName);
        emailInput.sendKeys(userEmail);
        passwordInput.sendKeys(this.userPassword);

        submitButton.click();
        expect(this.htmlBody.getText()).toContain(this.alert.alertMessages.registerSuccess);

        return userEmail;
    };

    this.changeAccountNames = function(firstName, lastName) {
        var firstNameInput = element(by.model('account.first_name'));
        var lastNameInput = element(by.model('account.last_name'));
        var saveButton = element(by.css('[form=accountForm]')).element(by.buttonText('Save'));

        this.get('/#/account');
        firstNameInput.clear().sendKeys(firstName);
        lastNameInput.clear().sendKeys(lastName);
        saveButton.click();

        this.alert.catchAlert( this.alert.alertMessages.accountSuccess, this.alert.alertTypes.success);
    };

    this.emailSubjects = {
        register: "Confirm your account",
        restorePass: "Restore your password"};

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

    var RestorePassObject = require('./restore_pass/po.js');
    this.restorePassword = function(userEmail, newPassword) {
        this.restorePassObj = new RestorePassObject();
        browser.get('/#/restore_password/');
        browser.waitForAngular();
        this.restorePassObj.getRestorePassPage(userEmail).then(function() {
            self.restorePassObj.setNewPassword(newPassword);
        });
    };

    this.isSubstr = function(string, substring) {
        if (string.indexOf(substring) > -1) return true;
    }
};

module.exports = Helper;