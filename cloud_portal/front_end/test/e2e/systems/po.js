'use strict';

var SystemsListPage = function () {

    this.getHomePage = function () {
        browser.get('/');
        browser.waitForAngular();
    };

    this.get = function () {
        browser.get('/#/systems');
        browser.waitForAngular();
    };

    this.htmlBody = element(by.css('body'));

    this.login = function() {
        var userEmail = 'ekorneeva+1@networkoptix.com';
        var userPassword = 'qweasd123';

        var loginButton = element(by.linkText('Login'));
        var loginDialog = element(by.css('.modal-dialog'));
        var emailInput = loginDialog.element(by.model('auth.email'));
        var passwordInput = loginDialog.element(by.model('auth.password'));
        var dialogLoginButton = loginDialog.element(by.buttonText('Login'));

        loginButton.click();

        emailInput.sendKeys(userEmail);
        passwordInput.sendKeys(userPassword);

        dialogLoginButton.click();
        browser.sleep(2000); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is displayed on page
        expect(element.all(by.css('.auth-visible')).first().isDisplayed()).toBe(true);
    }

    this.systemsList = element.all(by.repeater('system in systems'));
};

module.exports = SystemsListPage;