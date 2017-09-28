'use strict';

var AccountPage = function () {
    var  p = this;

    var PasswordFieldSuite = require('../password_check.js');
    this.passwordField = new PasswordFieldSuite();

    var Helper = require('../helper.js');
    this.helper = new Helper();

    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();

    this.passwordUrl = '/account/password';
    this.homePageUrl = '/';

    this.get = function (url) {
        browser.get(url);
        browser.waitForAngular();
    };

    this.refresh = function () {
        browser.refresh();
        browser.waitForAngular();
    };

    this.navbar = element(by.css('header')).element(by.css('.navbar'));
    this.userAccountDropdownToggle = this.navbar.element(by.css('a[uib-dropdown-toggle]'));
    this.userAccountDropdownMenu = this.navbar.element(by.css('[uib-dropdown-menu]'));
    this.changePasswordLink = this.userAccountDropdownMenu.element(by.linkText('Change Password'));

    this.emailField = element(by.model('account.email'));
    this.firstNameInput = element(by.model('account.first_name'));
    this.lastNameInput = element(by.model('account.last_name'));
    this.subscribeCheckbox = element(by.model('account.subscribe'));

    this.saveButton = element(by.css('[form=accountForm]')).element(by.buttonText('Save'));

    this.htmlBody = element(by.css('body'));

    this.currentPasswordInput = element(by.model('pass.password'));
    this.passwordInput = element(by.model('pass.newPassword')).element(by.css('input[type=password]'));
    this.submitButton = element(by.css('[form=passwordForm]')).element(by.buttonText('Change Password'));

    this.invalidClassRequired = 'ng-invalid-required';

    this.fieldWrap = function(field) {
        return field.element(by.xpath('../..'));
    };

    this.checkInputInvalid = function (field, invalidClass) {
        expect(field.getAttribute('class')).toContain(invalidClass);
        expect(this.fieldWrap(field).getAttribute('class')).toContain('has-error');
    };

    this.prepareToPasswordCheck = function () {
        this.currentPasswordInput.sendKeys(p.helper.userPassword);
    };

    this.passCheck = {
        input: this.passwordInput,
        submit: this.submitButton
    };
};

module.exports = AccountPage;