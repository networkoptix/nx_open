'use strict';

var RegisterPage = function () {

    var PasswordFieldSuite = require('../password_check.js');
    this.passwordField = new PasswordFieldSuite();

    var Helper = require('../helper.js');
    this.helper = new Helper();

    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();

    this.url = '/#/register';

    this.getHomePage = function () {
        browser.get('/');
        browser.waitForAngular();
    };

    this.getByUrl = function () {
        browser.get(this.url);
        browser.waitForAngular();
    };

    this.openRegisterButton = element(by.linkText('Register'));
    this.openRegisterButtonAdv = element(by.linkText('Register now')); // Register button on home page

    this.firstNameInput = element(by.model('account.firstName'));
    this.lastNameInput = element(by.model('account.lastName'));
    this.emailInput = element(by.model('account.email'));
    this.passwordGroup = element(by.css('password-input'));
    this.passwordInput = this.passwordGroup.element(by.css('input[type=password]'));

    this.submitButton = element(by.css('[form=registerForm]')).element(by.buttonText('Register'));

    this.htmlBody = element(by.css('body'));

    this.fieldWrap = function(field) {
        return field.element(by.xpath('../..'));
    };

    this.invalidClassRequired = 'ng-invalid-required';
    this.invalidClass = 'ng-invalid';
    this.invalidClassExists = 'ng-invalid-already-exists';

    this.checkInputInvalid = function (field, invalidClass) {
        expect(field.getAttribute('class')).toContain(invalidClass);
        expect(this.fieldWrap(field).getAttribute('class')).toContain('has-error');
    };

    this.checkPasswordInvalid = function (invalidClass) {
        expect(this.passwordInput.getAttribute('class')).toContain(invalidClass);
        expect(this.passwordGroup.$('.form-group').getAttribute('class')).toContain('has-error');
    };

    this.checkInputValid = function (field, invalidClass) {
        expect(field.getAttribute('class')).not.toContain(invalidClass);
        expect(this.fieldWrap(field).getAttribute('class')).not.toContain('has-error');
    };

    this.checkPasswordValid = function (invalidClass) {
        expect(this.passwordInput.getAttribute('class')).not.toContain(invalidClass);
        expect(this.passwordGroup.$('.form-group').getAttribute('class')).not.toContain('has-error');
    };

    this.checkEmailExists = function () {
        this.checkInputInvalid(this.emailInput, this.invalidClassExists);
        expect(this.fieldWrap(this.emailInput).$('.help-block').getText()).toContain('Email is already registered in portal');
    };

    this.passCheck = {
        input: this.passwordInput,
        submit: this.submitButton
    };

    this.prepareToPasswordCheck = function () {
        this.firstNameInput.sendKeys(this.helper.userFirstName);
        this.lastNameInput.sendKeys(this.helper.userLastName);
        this.emailInput.sendKeys(this.helper.getRandomEmail());
    };

    this.prepareToAlertCheck = function () {
        this.firstNameInput.sendKeys(this.helper.userFirstName);
        this.lastNameInput.sendKeys(this.helper.userLastName);
        this.emailInput.sendKeys(this.helper.getRandomEmail());
        this.passwordInput.sendKeys(this.helper.userPassword);
    };

    this.getActivationPage = function(userEmail) {
        var deferred = protractor.promise.defer();

        browser.controlFlow().wait(this.helper.getEmailTo(userEmail, this.helper.emailSubjects.register).then(function (email) {
            // extract registration token from the link in the email message
            var pattern = /\/static\/index.html#\/activate\/(\w+)/g;
            var regCode = pattern.exec(email.html)[1];
            console.log(regCode);
            browser.get('/#/activate/' + regCode);

            deferred.fulfill();
        }));

        return deferred.promise;
    };

    this.termsConditions = element(by.linkText('Terms and Conditions'));
};

module.exports = RegisterPage;