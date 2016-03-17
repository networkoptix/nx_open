'use strict';

var AccountPage = function () {

    var PasswordFieldSuite = require('../password_check.js');
    this.passwordField = new PasswordFieldSuite();

    this.passwordUrl = '/#/account/password';

    this.getHomePage = function () {
        browser.get('/');
        browser.waitForAngular();
    };

    this.getByUrl = function () {
        browser.get('/#/account');
        browser.waitForAngular();
    };

    this.getPasswordByUrl = function () {
        browser.get(this.passwordUrl);
        browser.waitForAngular();
    };

    this.refresh = function () {
        browser.refresh();
        browser.waitForAngular();
    }

    this.userEmail = 'ekorneeva+1@networkoptix.com';
    this.userPassword = 'qweasd123';

    this.userFirstName = 'TestFirstName';
    this.userLastName = 'TestLastName';

    this.userNameCyrillic = 'Кенгшщзх';
    this.userNameSmile = '☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★';
    this.userNameHierog = '您都可以享受源源不絕的好禮及優惠';

    this.navbar = element(by.css('header')).element(by.css('.navbar'));
    this.userAccountDropdownToggle = this.navbar.element(by.css('a[uib-dropdown-toggle]'));
    this.userAccountDropdownMenu = this.navbar.element(by.css('[uib-dropdown-menu]'));
    this.logoutLink = this.userAccountDropdownMenu.element(by.linkText('Logout'));
    this.changePasswordLink = this.userAccountDropdownMenu.element(by.linkText('Change Password'));

    this.loginSuccessElement = element.all(by.css('.auth-visible')).first(); // some element on page, that is only visible when user is authenticated

    this.login = function() {

        var loginButton = element(by.linkText('Login'));
        var loginDialog = element(by.css('.modal-dialog'));
        var emailInput = loginDialog.element(by.model('auth.email'));
        var passwordInput = loginDialog.element(by.model('auth.password'));
        var dialogLoginButton = loginDialog.element(by.buttonText('Login'));

        loginButton.click();

        emailInput.sendKeys(this.userEmail);
        passwordInput.sendKeys(this.userPassword);

        dialogLoginButton.click();
        browser.sleep(2000); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is displayed on page
        expect(this.loginSuccessElement.isDisplayed()).toBe(true);
    };

    this.logout = function() {
        expect(this.userAccountDropdownToggle.isDisplayed()).toBe(true);

        this.userAccountDropdownToggle.click();
        this.logoutLink.click();
        browser.sleep(500); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(this.loginSuccessElement.isDisplayed()).toBe(false);
    }

    this.emailField = element(by.model('account.email'));
    this.firstNameInput = element(by.model('account.first_name'));
    this.lastNameInput = element(by.model('account.last_name'));
    this.subscribeCheckbox = element(by.model('account.subscribe'));

    this.saveButton = element(by.css('[form=accountForm]')).element(by.buttonText('Save'));

    this.saveSuccessAlert = element(by.css('process-alert[process=save]')).element(by.css('.alert')); // alert with success message
    this.passwordCahngeSuccessAlert = element(by.css('process-alert[process=changePassword]')).element(by.css('.alert')); // alert with success message
    this.catchSuccessAlert = function (alertElement, message) {
        // Workaround due to Protractor bug with timeouts https://github.com/angular/protractor/issues/169
        // taken from here http://stackoverflow.com/questions/25062748/testing-the-contents-of-a-temporary-element-with-protractor
        browser.sleep(1500);
        browser.ignoreSynchronization = true;
        expect(alertElement.getText()).toContain(message);
        browser.sleep(500);
        browser.ignoreSynchronization = false;
    }

    this.htmlBody = element(by.css('body'));

    this.currentPasswordInput = element(by.model('pass.password'));
    this.passwordInput = element(by.model('pass.newPassword')).element(by.css('input[type=password]'));
    this.submitButton = element(by.css('[form=passwordForm]')).element(by.buttonText('Change password'));

    this.prepareToPasswordCheck = function () {
        this.currentPasswordInput.sendKeys(this.userPassword);
    }

    this.passwordGroup = element(by.css('password-input'));
    this.passwordControlContainer = this.passwordGroup.element(by.css('.help-block'));


};

module.exports = AccountPage;