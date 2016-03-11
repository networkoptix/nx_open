'use strict';

var LoginPage = function () {

    this.getHomePage = function () {
        browser.get('/');
        browser.waitForAngular();
    };

    this.getByTopButton = function () {
        browser.get('/');
        browser.waitForAngular();

        this.openRegisterButton.click();
    };

    this.getByLink = function () {
        browser.get('/#/register');
        browser.waitForAngular();
    };

    var randomNumber = Math.floor((Math.random() * 100)+10); // Random number between 10 and 100
    this.userEmailRandom = 'ekorneeva+' + randomNumber + '@networkoptix.com'; // valid email with random number between 10 and 100
    this.userFirstNameRandom = 'TestFirstName' + randomNumber;
    this.userLastName = 'TestLastName';
    this.userPassword = 'qweasd123';

    this.openRegisterButton = element(by.linkText('Register'));
    this.openRegisterButtonAdv = element(by.linkText('Register immediately!')); // Register button on home page

    this.firstNameInput = element(by.model('account.firstName'));
    this.lastNameInput = element(by.model('account.lastName'));
    this.emailInput = element(by.model('account.email'));
    this.passwordInput = element(by.model('account.password')).element(by.css('input[type=password]'));

    this.submitRegisterButton = element(by.css('[form=registerForm]')).element(by.buttonText('Register'));

    this.htmlBody = element(by.css('body'));

    this.registerSuccessAlert = element(by.css('process-alert[process=register]')).element(by.css('.alert')); // alert with success message
    this.catchregisterSuccessAlert = function (registerSuccessAlert) {
        // Workaround due to Protractor bug with timeouts https://github.com/angular/protractor/issues/169
        // taken from here http://stackoverflow.com/questions/25062748/testing-the-contents-of-a-temporary-element-with-protractor
        browser.sleep(500);
        browser.ignoreSynchronization = true;
        expect(registerSuccessAlert.getText()).toContain('Your account was successfully registered. Please, check your email to confirm it');
        browser.sleep(500);
        browser.ignoreSynchronization = false;
    }
};

module.exports = LoginPage;