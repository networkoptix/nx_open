'use strict';
var RegisterPage = require('./po.js');
describe('Registration suite', function () {
    var p = new RegisterPage();

    beforeAll(function() {
        console.log('Registration suite start');
    });

    afterAll(function() {
        console.log('Registration suite finish');
    });

    beforeEach(function() {
        p.helper.get(p.url);
    });

    it("should open register page in anonymous state by clicking Register button on top right corner", function () {
        p.helper.get('/');
    
        p.openRegisterButton.click();
    
        expect(browser.getCurrentUrl()).toContain('register');
        expect(p.helper.htmlBody.getText()).toContain('Welcome to Nx Cloud');
    });

    it("should open register page from register success page by clicking Register button on top right corner", function () {
        p.helper.get('/');

        p.openRegisterButton.click();

        expect(browser.getCurrentUrl()).toContain('register');
        expect(p.helper.htmlBody.getText()).toContain('Welcome to Nx Cloud');
    });

    it("should open register page in anonymous state by clicking Register button on homepage", function () {
        p.helper.get('/');

        p.openRegisterButtonAdv.click();

        expect(browser.getCurrentUrl()).toContain('register');
        expect(p.helper.htmlBody.getText()).toContain('Welcome to Nx Cloud');
    });

    it("should open register page in anonymous state", function () {
        expect(browser.getCurrentUrl()).toContain('register');
        expect(p.helper.htmlBody.getText()).toContain('Welcome to Nx Cloud');
    });

    it("should register user with correct credentials", function () {
        p.firstNameInput.sendKeys(p.helper.userFirstName);
        p.lastNameInput.sendKeys(p.helper.userLastName);
        p.emailInput.sendKeys(p.helper.getRandomEmail());
        p.passwordInput.sendKeys(p.helper.userPassword);

        p.submitButton.click();
        expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerSuccess);
    });

    it("should register user with cyrillic First and Last names and correct credentials", function () {
        p.firstNameInput.sendKeys(p.helper.userNameCyrillic);
        p.lastNameInput.sendKeys(p.helper.userNameCyrillic);
        p.emailInput.sendKeys(p.helper.getRandomEmail());
        p.passwordInput.sendKeys(p.helper.userPassword);

        p.submitButton.click();
        expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerSuccess);
    });

    it("should register user with smile symbols in First and Last name fields and correct credentials", function () {
        p.firstNameInput.sendKeys(p.helper.userNameSmile);
        p.lastNameInput.sendKeys(p.helper.userNameSmile);
        p.emailInput.sendKeys(p.helper.getRandomEmail());
        p.passwordInput.sendKeys(p.helper.userPassword);

        p.submitButton.click();
        expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerSuccess);
    });

    it("should register user with hieroglyphic symbols in First and Last name fields and correct credentials", function () {
        p.firstNameInput.sendKeys(p.helper.userNameHierog);
        p.lastNameInput.sendKeys(p.helper.userNameHierog);
        p.emailInput.sendKeys(p.helper.getRandomEmail());
        p.passwordInput.sendKeys(p.helper.userPassword);

        p.submitButton.click();
        expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerSuccess);
    });

    it("should not allow to register without all fields filled", function () {
        p.submitButton.click();

        // Check that all input fields are marked as incorrect
        p.checkInputInvalid(p.firstNameInput, p.invalidClassRequired);
        p.checkInputInvalid(p.lastNameInput, p.invalidClassRequired);
        p.checkInputInvalid(p.emailInput, p.invalidClassRequired);
        p.checkPasswordInvalid(p.invalidClassRequired);
    });

    it("should not allow to register with blank spaces in in First and Last name fields", function () {
        p.firstNameInput.sendKeys(' ');
        p.lastNameInput.sendKeys(' ');
        p.emailInput.sendKeys(p.helper.getRandomEmail());
        p.passwordInput.sendKeys(p.helper.userPassword);

        p.submitButton.click();

        p.checkInputInvalid(p.firstNameInput, p.invalidClassRequired);
        p.checkInputInvalid(p.lastNameInput, p.invalidClassRequired);
    });

    it("should allow  `~!@#$%^&*()_:\";\'{}[]+<>?,./  in First and Last name fields", function () {
        p.firstNameInput.sendKeys(p.helper.inputSymb);
        p.lastNameInput.sendKeys(p.helper.inputSymb);
        p.emailInput.sendKeys(p.helper.getRandomEmail());
        p.passwordInput.sendKeys(p.helper.userPassword);

        p.submitButton.click();
        expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerSuccess);
    });

    it("should allow `~!@#$%^&*()_:\";\'{}[]+<>?,./  in email field", function () {
        p.firstNameInput.sendKeys(p.helper.userFirstName);
        p.lastNameInput.sendKeys(p.helper.userLastName);
        p.passwordInput.sendKeys(p.helper.userPassword);

        p.emailInput.sendKeys(p.helper.getRandomEmailWith('#!$%&\'*+-/=?^_\`{}|~@gmail.com'));

        p.submitButton.click();
        expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerSuccess);
    });



    it("should not allow to register without email", function () {
        p.firstNameInput.sendKeys(p.helper.userFirstName);
        p.lastNameInput.sendKeys(p.helper.userLastName);
        p.passwordInput.sendKeys(p.helper.userPassword);

        p.submitButton.click();

        // Check that all email field is marked as incorrect
        p.checkInputInvalid(p.emailInput, p.invalidClassRequired);

        // Check that all other input fields are not marked as incorrect
        p.checkInputValid(p.firstNameInput, p.invalidClassRequired);
        p.checkInputValid(p.lastNameInput, p.invalidClassRequired);
        p.checkPasswordValid(p.invalidClassRequired);
    });

    it("should respond to Enter key and save data", function () {
        p.firstNameInput.sendKeys(p.helper.userFirstName);
        p.lastNameInput.sendKeys(p.helper.userLastName);
        p.emailInput.sendKeys(p.helper.getRandomEmail());
        p.passwordInput.sendKeys(p.helper.userPassword)
            .sendKeys(protractor.Key.ENTER);
        expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerSuccess);
    });

    it("should respond to Tab key", function () {
        // Navigate to next field using TAB key
        p.firstNameInput.sendKeys(protractor.Key.TAB);
        p.helper.checkElementFocusedBy(p.lastNameInput, 'id');
    });

    it("should not allow to register with email in non-email format", function () {
        p.firstNameInput.sendKeys(p.helper.userFirstName);
        p.lastNameInput.sendKeys(p.helper.userLastName);
        p.passwordInput.sendKeys(p.helper.userPassword);

        p.checkEmail('noptixqagmail.com');
        //p.checkEmail('noptixqa@gmailcom');
        //p.checkEmail('noptixqa@gmail.');
        p.checkEmail('@gmail.com');
        p.checkEmail('noptixqa@gmail..com');
        p.checkEmail('noptixqa@192.168.1.1.0');
        p.checkEmail('noptixqa.@gmail.com');
        p.checkEmail('noptixq..a@gmail.c');
        //p.checkEmail('noptixqa<noptixqa@gmail.com>');
        //p.checkEmail('noptixqa@noptixqa@gmail.com');
        p.checkEmail('noptixqa@-gmail.com');
        //p.checkEmail('noptixqa@gmail.com(noptixqa)');
    });

    p.passwordField.check(function(){
        var deferred = protractor.promise.defer();
        p.prepareToPasswordCheck();
        deferred.fulfill();
        return deferred.promise;
    }, p);

    it("should open Terms and conditions in a new page", function () {
        p.termsConditions.click();

        // Switch to just opened new tab
        browser.getAllWindowHandles().then(function (handles) {
            var newWindowHandle = handles[1];
            browser.switchTo().window(newWindowHandle).then(function () {
                expect(browser.getCurrentUrl()).toContain('static/eula'); // Check that url is correct
                expect(p.helper.htmlBody.getText()).toContain('Terms and conditions'); // Check that it is Terms and conditions page
            });
        });
    });

    it("should not allow registration with existing email and show error", function () {
        p.firstNameInput.sendKeys(p.helper.userFirstName);
        p.lastNameInput.sendKeys(p.helper.userLastName);
        p.emailInput.sendKeys(p.helper.userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword);

        p.submitButton.click();

        p.checkEmailExists();
    });
});
