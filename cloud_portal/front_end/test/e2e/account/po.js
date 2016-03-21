'use strict';

var AccountPage = function () {

    var PasswordFieldSuite = require('../password_check.js');
    this.passwordField = new PasswordFieldSuite();

    var Helper = require('../helper.js');
    this.helper = new Helper();

    this.accountUrl = '/#/account';
    this.passwordUrl = '/#/account/password';
    this.homePageUrl = '/';

    this.get = function (url) {
        browser.get(url);
        browser.waitForAngular();
    };

    this.refresh = function () {
        browser.refresh();
        browser.waitForAngular();
    }

    this.userEmail = 'ekorneeva+1@networkoptix.com';
    this.userPassword = 'qweasd123';
    this.userPasswordNew = 'qweasd123qwe';
    this.userPasswordWrong = 'qweqwe123';

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

    this.emailField = element(by.model('account.email'));
    this.firstNameInput = element(by.model('account.first_name'));
    this.lastNameInput = element(by.model('account.last_name'));
    this.subscribeCheckbox = element(by.model('account.subscribe'));

    this.saveButton = element(by.css('[form=accountForm]')).element(by.buttonText('Save'));

    this.saveSuccessAlert = element(by.css('process-alert[process=save]')).element(by.css('.alert')); // alert with success message
    this.passwordChangeAlert = element(by.css('process-alert[process=changePassword]')).element(by.css('.alert')); // alert with success message

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