'use strict';

var RegisterPage = function () {

    var p = this;

    var PasswordFieldSuite = require('../password_check.js');
    this.passwordField = new PasswordFieldSuite();

    var Helper = require('../helper.js');
    this.helper = new Helper();

    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();

    this.url = '/register';

    this.openRegisterButton = p.helper.forms.register.triggerRegisterButton;
    this.openRegisterButtonAdv = element(by.linkText('Create account now')); // Register button on home page

    this.firstNameInput = element(by.model('account.firstName'));
    this.lastNameInput = element(by.model('account.lastName'));
    this.emailInput = element(by.model('account.email'));
    this.passwordGroup = element(by.css('password-input'));
    this.passwordInput = this.passwordGroup.element(by.css('input[type=password]'));

    this.submitButton = p.helper.forms.register.submitButton;

    this.openInClientButton = element(by.cssContainingText('button', 'Open '));
    this.messageLoginLink = p.helper.forms.restorePassPassword.messageLoginLink;

    this.fieldWrap = function(field) {
        return field.element(by.xpath('../..'));
    };

    this.preRegisterMessage = "By clicking 'Create Account', you agree\nto Terms and Conditions";

    this.invalidClassRequired = 'ng-invalid-required';
    this.invalidClass = 'ng-invalid';
    this.invalidClassExists = 'ng-invalid-already-exists';

    this.checkInputInvalid = function (field, invalidClass) {
        field.getAttribute('class');
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
        expect(this.fieldWrap(this.emailInput).$('.help-block').getText()).toContain('This email address has been already registered in');
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

    this.getActivationLink = function(userEmail) {
        var deferred = protractor.promise.defer();

        browser.controlFlow().wait(this.helper.getEmailTo(userEmail, this.helper.emailSubjects.register).then(function (email) {
            // extract registration token from the link in the email message
            var pattern = new RegExp("activate/(\\w+)", "g");
            var regCode = pattern.exec(email.html)[1];
            console.log(regCode);

            deferred.fulfill('/activate/' + regCode);
        }));

        return deferred.promise;
    };

    this.termsConditions = element(by.linkText('Terms and Conditions'));

    this.checkEmail = function (email) {
        this.emailInput.clear().sendKeys(email);
        this.submitButton.click();
        this.checkInputInvalid(this.emailInput, this.invalidClass);
    };

    this.accountFirstNameInput = element(by.model('account.first_name'));
    this.accountLastNameInput = element(by.model('account.last_name'));
};

module.exports = RegisterPage;