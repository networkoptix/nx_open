'use strict';

var RegisterPage = function () {

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

    // Get valid email with random number between 100 and 1000
    this.getRandomEmail = function() {
        var randomNumber = Math.floor((Math.random() * 1000)+100); // Random number between 100 and 1000
        return 'ekorneeva+' + randomNumber + '@networkoptix.com';
    }
    this.userEmailExisting = 'ekorneeva+1@networkoptix.com'; // valid existing email
    this.userFirstName = 'TestFirstName';
    this.userLastName = 'TestLastName';
    this.userPassword = 'qweasd123';

    this.userNameCyrillic = 'Кенгшщзх';
    this.userNameSmile = '☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★';
    this.userNameHierog = '您都可以享受源源不絕的好禮及優惠';

    this.userPasswordCyrillic = 'йцуфывячс';
    this.userPasswordSmile = '☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★';
    this.userPasswordHierog = '您都可以享受源源不絕的好禮及優惠';

    this.openRegisterButton = element(by.linkText('Register'));
    this.openRegisterButtonAdv = element(by.linkText('Register immediately!')); // Register button on home page

    this.firstNameInput = element(by.model('account.firstName'));
    this.lastNameInput = element(by.model('account.lastName'));
    this.emailInput = element(by.model('account.email'));
    this.passwordGroup = element(by.model('account.password'));
    this.passwordInput = this.passwordGroup.element(by.css('input[type=password]'));

    this.submitRegisterButton = element(by.css('[form=registerForm]')).element(by.buttonText('Register'));

    this.htmlBody = element(by.css('body'));

    this.registerSuccessAlert = element(by.css('process-alert[process=register]')).element(by.css('.alert')); // alert with success message
    this.catchRegisterSuccessAlert = function (registerSuccessAlert) {
        // Workaround due to Protractor bug with timeouts https://github.com/angular/protractor/issues/169
        // taken from here http://stackoverflow.com/questions/25062748/testing-the-contents-of-a-temporary-element-with-protractor
        browser.sleep(1500);
        browser.ignoreSynchronization = true;
        expect(registerSuccessAlert.getText()).toContain('Your account was successfully registered. Please, check your email to confirm it');
        browser.sleep(500);
        browser.ignoreSynchronization = false;
    }

    this.fieldWrap = function(field) {
        return field.element(by.xpath('../..'));
    }

    this.invalidClassRequired = 'ng-invalid-required';
    this.invalidClass = 'ng-invalid';
    this.invalidClassExists = 'ng-invalid-already-exists';

    this.checkInputInvalid = function(field, invalidClass) {
        expect(field.getAttribute('class')).toContain(invalidClass);
        expect(this.fieldWrap(field).getAttribute('class')).toContain('has-error');
    }

    this.checkPasswordInvalid = function(invalidClass) {
        expect(this.passwordInput.getAttribute('class')).toContain(invalidClass);
        expect(this.passwordGroup.$('.form-group').getAttribute('class')).toContain('has-error');
    }

    this.checkInputValid = function(field, invalidClass) {
        expect(field.getAttribute('class')).not.toContain(invalidClass);
        expect(this.fieldWrap(field).getAttribute('class')).not.toContain('has-error');
    }

    this.checkPasswordValid = function(invalidClass) {
        expect(this.passwordInput.getAttribute('class')).not.toContain(invalidClass);
        expect(this.passwordGroup.$('.form-group').getAttribute('class')).not.toContain('has-error');
    }

    this.checkEmailExists = function() {
        this.checkInputInvalid(this.emailInput, this.invalidClassExists);
        expect(this.fieldWrap(this.emailInput).$('.help-block').getText()).toContain('Email is already registered in portal');
    }

    this.passwordControlContainer = this.passwordGroup.element(by.css('.help-block'));
    this.passwordWeak = { class:'label-danger', text:'too short', title: 'Password must contain at least 6 characters' };
    this.passwordCommon = { class:'label-danger', text:'too common', title: 'This password is in top most popular passwords in the world' };
    this.passwordFair = { class:'label-warning', text:'fair', title: 'Use numbers, symbols in different case and special symbols to make your password stronger' };
    this.passwordGood = { class:'label-success', text:'good', title: '' };

    this.checkPasswordWarning = function(strength) {
        expect(this.passwordControlContainer.element(by.css('.label')).getAttribute('class')).toContain(strength.class);
        expect(this.passwordControlContainer.element(by.css('.label')).getText()).toContain(strength.text);
    }

    this.termsConditions = element(by.linkText('Terms and Conditions'));
};

module.exports = RegisterPage;