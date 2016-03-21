'use strict';
var RegisterPage = require('./po.js');
describe('Registration suite', function () {

    var p = new RegisterPage();

    it("should open register page in anonymous state by clicking Register button on top right corner", function () {
        p.getHomePage();

        p.openRegisterButton.click();

        expect(browser.getCurrentUrl()).toContain('register');
        expect(p.htmlBody.getText()).toContain('Register to be happy');
    });

    it("should open register page in anonymous state by clicking Register button on homepage", function () {
        p.getHomePage();

        p.openRegisterButtonAdv.click();

        expect(browser.getCurrentUrl()).toContain('register');
        expect(p.htmlBody.getText()).toContain('Register to be happy');
    });

    it("should open register page in anonymous state", function () {
        p.getByUrl();

        expect(browser.getCurrentUrl()).toContain('register');
        expect(p.htmlBody.getText()).toContain('Register to be happy');
    });

    it("should register user with correct credentials", function () {
        p.getByUrl();

        p.firstNameInput.sendKeys(p.userFirstName);
        p.lastNameInput.sendKeys(p.userLastName);
        p.emailInput.sendKeys(p.getRandomEmail());
        p.passwordInput.sendKeys(p.userPassword);

        p.submitButton.click();

        p.helper.catchAlert(p.registerSuccessAlert, 'Your account was successfully registered. Please, check your email to confirm it');

        // Check that registration form element is NOT displayed on page
        expect(p.firstNameInput.isPresent()).toBe(false);
    });

    it("should register user with cyrillic First and Last names and correct credentials", function () {
        p.getByUrl();

        p.firstNameInput.sendKeys(p.userNameCyrillic);
        p.lastNameInput.sendKeys(p.userNameCyrillic);
        p.emailInput.sendKeys(p.getRandomEmail());
        p.passwordInput.sendKeys(p.userPassword);

        p.submitButton.click();

        p.helper.catchAlert(p.registerSuccessAlert, 'Your account was successfully registered. Please, check your email to confirm it');

        // Check that registration form element is NOT displayed on page
        expect(p.firstNameInput.isPresent()).toBe(false);
    });

    it("should register user with smile symbols in First and Last name fields and correct credentials", function () {
        p.getByUrl();

        p.firstNameInput.sendKeys(p.userNameSmile);
        p.lastNameInput.sendKeys(p.userNameSmile);
        p.emailInput.sendKeys(p.getRandomEmail());
        p.passwordInput.sendKeys(p.userPassword);

        p.submitButton.click();

        p.helper.catchAlert(p.registerSuccessAlert, 'Your account was successfully registered. Please, check your email to confirm it');

        // Check that registration form element is NOT displayed on page
        expect(p.firstNameInput.isPresent()).toBe(false);
    });

    it("should register user with hieroglyphic symbols in First and Last name fields and correct credentials", function () {
        p.getByUrl();

        p.firstNameInput.sendKeys(p.userNameHierog);
        p.lastNameInput.sendKeys(p.userNameHierog);
        p.emailInput.sendKeys(p.getRandomEmail());
        p.passwordInput.sendKeys(p.userPassword);

        p.submitButton.click();

        p.helper.catchAlert(p.registerSuccessAlert, 'Your account was successfully registered. Please, check your email to confirm it');

        // Check that registration form element is NOT displayed on page
        expect(p.firstNameInput.isPresent()).toBe(false);
    });

    it("should not allow to register without all fields filled", function () {
        p.getByUrl();
        p.submitButton.click();

        // Check that all input fields are marked as incorrect
        p.checkInputInvalid(p.firstNameInput, p.invalidClassRequired);
        p.checkInputInvalid(p.lastNameInput, p.invalidClassRequired);
        p.checkInputInvalid(p.emailInput, p.invalidClassRequired);
        p.checkPasswordInvalid(p.invalidClassRequired);
    });

    it("should not allow to register without email", function () {
        p.getByUrl();

        p.firstNameInput.sendKeys(p.userFirstName);
        p.lastNameInput.sendKeys(p.userLastName);
        p.passwordInput.sendKeys(p.userPassword);

        p.submitButton.click();

        // Check that all email field is marked as incorrect
        p.checkInputInvalid(p.emailInput, p.invalidClassRequired);

        // Check that all other input fields are not marked as incorrect
        p.checkInputValid(p.firstNameInput, p.invalidClassRequired);
        p.checkInputValid(p.lastNameInput, p.invalidClassRequired);
        p.checkPasswordValid(p.invalidClassRequired);
    });

    it("should not allow to register with email in non-email format", function () {
        p.getByUrl();

        p.firstNameInput.sendKeys(p.userFirstName);
        p.lastNameInput.sendKeys(p.userLastName);
        p.emailInput.sendKeys('vert546 464w6345');
        p.passwordInput.sendKeys(p.userPassword);

        p.submitButton.click();

        p.checkInputInvalid(p.emailInput, p.invalidClass);
    });

    p.passwordField.check(p, p.url);

    it("should open Terms and conditions in a new page", function () {
        p.getByUrl();
        p.termsConditions.click();

        // Switch to just opened new tab
        browser.getAllWindowHandles().then(function (handles) {
            var newWindowHandle = handles[1];
            browser.switchTo().window(newWindowHandle).then(function () {
                expect(browser.getCurrentUrl()).toContain('static/eula'); // Check that url is correct
                expect(p.htmlBody.getText()).toContain('Terms and conditions'); // Check that it is Terms and conditions page
            });
        });
    });

    it("should not allow registration with existing email and show error", function () {
        p.getByUrl();

        p.firstNameInput.sendKeys(p.userFirstName);
        p.lastNameInput.sendKeys(p.userLastName);
        p.emailInput.sendKeys(p.userEmailExisting);
        p.passwordInput.sendKeys(p.userPassword);

        p.submitButton.click();

        p.checkEmailExists();
    });
});