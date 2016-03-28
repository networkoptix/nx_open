'use strict';

var LoginPage;
LoginPage = function () {

    var Helper = require('../helper.js');
    this.helper = new Helper();

    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();

    this.get = function () {
        browser.get('/');
        browser.waitForAngular();
        browser.sleep(500);
        this.loginButton.click();
    };

    this.loginButton = element(by.linkText('Login'));

    this.loginDialog = element(by.css('.modal-dialog'));

    this.emailInput = this.loginDialog.element(by.model('auth.email'));
    this.passwordInput = this.loginDialog.element(by.model('auth.password'));

    this.emailInputWrap = this.emailInput.element(by.xpath('..'));
    this.passwordInputWrap = this.passwordInput.element(by.xpath('..'));

    this.checkPasswordMissing = function () {
        expect(this.passwordInput.getAttribute('class')).toContain('ng-invalid-required');
        expect(this.passwordInputWrap.getAttribute('class')).toContain('has-error');
    };

    this.checkEmailMissing = function () {
        expect(this.emailInput.getAttribute('class')).toContain('ng-invalid-required');
        expect(this.emailInputWrap.getAttribute('class')).toContain('has-error');
    };

    this.checkEmailInvalid = function () {
        expect(this.emailInput.getAttribute('class')).toContain('ng-invalid');
        expect(this.emailInputWrap.getAttribute('class')).toContain('has-error');
    };

    this.dialogLoginButton = this.loginDialog.element(by.buttonText('Login'));
    this.dialogCloseButton = this.loginDialog.all(by.css('button.close')).first();

    this.loginSuccessElement = element.all(by.css('.auth-visible')).first(); // some element on page, that is only visible when user is authenticated

    this.loginDialogBackground = element(by.css('.modal')); // login dialog overlay

    this.navbar = element(by.css('header')).element(by.css('.navbar'));
    this.userAccountDropdownToggle = this.navbar.element(by.css('a[uib-dropdown-toggle]'));
    this.userAccountDropdownMenu = this.navbar.element(by.css('[uib-dropdown-menu]'));
    this.userLogoutLink = this.userAccountDropdownMenu.element(by.linkText('Logout'));

    this.rememberCheckbox = element(by.model('auth.remember'));
    this.iForgotPasswordLink = element(by.linkText('I forgot my password'));

    this.htmlBody = element(by.css('body'));

    this.restoreEmailInput = element(by.model('data.email'));
    this.newPasswordInput = element(by.model('data.newPassword')).element(by.css('input[type=password]'));
    this.submitButton = element(by.buttonText('Restore password'));
    this.savePasswordButton = element(by.buttonText('Save password'));

    this.login = function () {
        this.dialogLoginButton.click();
        browser.sleep(2000); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is displayed on page
        expect(this.loginSuccessElement.isDisplayed()).toBe(true);
    };
    this.logout = function () {
        expect(this.userAccountDropdownToggle.isDisplayed()).toBe(true);

        this.userAccountDropdownToggle.click();
        this.userLogoutLink.click();
        browser.sleep(500); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(this.loginSuccessElement.isDisplayed()).toBe(false);
    };
};

module.exports = LoginPage;