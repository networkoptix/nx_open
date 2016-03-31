'use strict';

var Helper = function () {
    var AlertSuite = require('./alerts_check.js');
    this.alert = new AlertSuite();

    var self = this;

    this.baseEmail = 'noptixqa@gmail.com';
    this.basePassword = 'qweasd123';

    // Get valid email with random number between 100 and 1000
    this.getRandomEmail = function() {
        var randomNumber = Math.floor((Math.random() * 10000)+1000); // Random number between 1000 and 10000
        return 'noptixqa+' + randomNumber + '@gmail.com';
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

    this.userPasswordCyrillic = 'йцуфывячс';
    this.userPasswordSmile = '☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★';
    this.userPasswordHierog = '您都可以享受源源不絕的好禮及優惠';
    this.userPasswordWrong = 'qweqwe123';

    this.loginSuccessElement = element.all(by.css('.auth-visible')).first(); // some element on page, that is only visible when user is authenticated

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

        this.alert.catchAlert( this.alert.alertMessages.registerSuccess, this.alert.alertTypes.success);

        // Check that registration form element is NOT displayed on page
        expect(firstNameInput.isPresent()).toBe(false);

        return userEmail;
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
    }
};

module.exports = Helper;