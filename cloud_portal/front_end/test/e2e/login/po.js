'use strict';

var LoginPage;
LoginPage = function () {

    var Helper = require('../helper.js');
    this.helper = new Helper();

    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();

    var p = this;

    this.get = function () {
        browser.get('/');
        browser.waitForAngular();
        browser.sleep(500);
    };

    this.loginButton = this.helper.forms.login.openLink;

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

    this.dialogLoginButton = this.helper.forms.login.submitButton;
    this.dialogCloseButton = this.loginDialog.all(by.css('button.close')).first();

    this.loginSuccessElement = p.helper.loginSuccessElement;
    this.loginDialogBackground = element(by.css('.modal')); // login dialog overlay

    this.navbar = this.helper.forms.logout.navbar;

    this.rememberCheckbox = element(by.model('auth.remember'));
    this.iForgotPasswordLink = element(by.linkText('Forgot password?'));

    this.htmlBody = element(by.css('body'));

    this.restoreEmailInput = element(by.model('data.email'));
    this.submitButton = element(by.buttonText('Restore password'));
    this.savePasswordButton = element(by.buttonText('Save password'));

    this.login = function () {
        this.dialogLoginButton.click();
        browser.sleep(5000);

        // Check that element that is visible only for authorized user is displayed on page
        expect(this.loginSuccessElement.isDisplayed()).toBe(true);
    };
};

module.exports = LoginPage;
