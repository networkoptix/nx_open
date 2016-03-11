'use strict';

var LoginPage = function () {

    this.get = function () {
        browser.get('/');
        browser.waitForAngular();

        this.loginButton.click();
    };

    this.userEmail1 = 'ekorneeva+1@networkoptix.com';
    this.userEmail2 = 'ekorneeva+2@networkoptix.com';
    this.userEmailWrong = 'nonexistingperson@networkoptix.com';
    this.userPassword = 'qweasd123';
    this.userPasswordWrong = 'qweqwe123';

    this.loginButton = element(by.linkText('Login'));

    this.loginDialog = element(by.css('.modal-dialog'));

    this.emailInput = this.loginDialog.element(by.model('auth.email'));
    this.passwordInput = this.loginDialog.element(by.model('auth.password'));

    this.dialogLoginButton = this.loginDialog.element(by.buttonText('Login'));
    this.dialogCloseButton = this.loginDialog.all(by.css('button.close')).first();

    this.loginIncorrectAlert = element(by.css('process-alert[process=login]')).element(by.css('.alert')); // alert with eror message
    this.catchLoginIncorrectAlert = function (loginIncorrectAlert) {
        // Workaround due to Protractor bug with timeouts https://github.com/angular/protractor/issues/169
        // taken from here http://stackoverflow.com/questions/25062748/testing-the-contents-of-a-temporary-element-with-protractor
        browser.sleep(500);
        browser.ignoreSynchronization = true;
        expect(loginIncorrectAlert.getText()).toContain('Login or password are incorrect');
        browser.sleep(500);
        browser.ignoreSynchronization = false;
    }

    this.loginSuccessElement = element.all(by.css('.auth-visible')).first(); // some element on page, that is only visible when user is authenticated

    this.loginDialogBackground = element(by.css('.modal')); // background of login dialog

    this.navbar = element(by.css('header')).element(by.css('.navbar'));
    this.userAccountDropdownToggle = this.navbar.element(by.css('a[uib-dropdown-toggle]'));
    this.userAccountDropdownMenu = this.navbar.element(by.css('[uib-dropdown-menu]'));
    this.userLogoutLink = this.userAccountDropdownMenu.element(by.linkText('Logout'));

    this.remembermeCheckbox = element(by.model('auth.remember'));
    this.iForgotPasswordLink = element(by.linkText('I forgot my password'));

    this.htmlBody = element(by.css('body'));

    this.restoreEmailInput = element(by.model('data.email'));

};

module.exports = LoginPage;