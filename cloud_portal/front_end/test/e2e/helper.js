'use strict';

var Helper = function () {

    this.login = function(email, password) {

        var loginButton = element(by.linkText('Login'));
        var loginDialog = element(by.css('.modal-dialog'));
        var emailInput = loginDialog.element(by.model('auth.email'));
        var passwordInput = loginDialog.element(by.model('auth.password'));
        var dialogLoginButton = loginDialog.element(by.buttonText('Login'));
        var loginSuccessElement = element.all(by.css('.auth-visible')).first(); // some element on page, that is only visible when user is authenticated


        browser.get('/');
        browser.waitForAngular();

        loginButton.click();

        emailInput.sendKeys(email);
        passwordInput.sendKeys(password);

        dialogLoginButton.click();
        browser.sleep(2000); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is displayed on page
        expect(loginSuccessElement.isDisplayed()).toBe(true);
    };

    this.logout = function() {
        var navbar = element(by.css('header')).element(by.css('.navbar'));
        var userAccountDropdownToggle = navbar.element(by.css('a[uib-dropdown-toggle]'));
        var userAccountDropdownMenu = navbar.element(by.css('[uib-dropdown-menu]'));
        var logoutLink = userAccountDropdownMenu.element(by.linkText('Logout'));
        var loginSuccessElement = element.all(by.css('.auth-visible')).first(); // some element on page, that is only visible when user is authenticated

        expect(userAccountDropdownToggle.isDisplayed()).toBe(true);

        userAccountDropdownToggle.click();
        logoutLink.click();
        browser.sleep(500); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(loginSuccessElement.isDisplayed()).toBe(false);
    };

    this.catchAlert = function (alertElement, message) {
        // Workaround due to Protractor bug with timeouts https://github.com/angular/protractor/issues/169
        // taken from here http://stackoverflow.com/questions/25062748/testing-the-contents-of-a-temporary-element-with-protractor
        browser.sleep(1500);
        browser.ignoreSynchronization = true;
        expect(alertElement.getText()).toContain(message);
        browser.sleep(500);
        browser.ignoreSynchronization = false;
    }
};

module.exports = Helper;