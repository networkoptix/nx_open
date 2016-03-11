'use strict';
var LoginPage = require('./po.js');
describe('Login suite', function () {

    var p = new LoginPage();

    it("should open register page in anonimous state by clicking Register button on top right corner", function () {
        p.getHomePage();

        p.openRegisterButton.click();

        expect(browser.getCurrentUrl()).toContain('register');
        expect(p.htmlBody.getText()).toContain('Register to be happy');
    });

    it("should open register page in anonimous state by clicking Register button on homepage", function () {
        p.getHomePage();

        p.openRegisterButtonAdv.click();

        expect(browser.getCurrentUrl()).toContain('register');
        expect(p.htmlBody.getText()).toContain('Register to be happy');
    });

    it("should open register page in anonimous state", function () {
        p.getByLink();

        expect(browser.getCurrentUrl()).toContain('register');
        expect(p.htmlBody.getText()).toContain('Register to be happy');
    });

    it("should register user with correct credentials", function () {
        p.getByLink();

        p.firstNameInput.sendKeys(p.userFirstNameRandom);
        p.lastNameInput.sendKeys(p.userLastName);
        p.emailInput.sendKeys(p.userEmailRandom);
        p.passwordInput.sendKeys(p.userPassword);

        p.htmlBody.click(); // this should be added because of strange behaviour; could be removed later
        p.submitRegisterButton.click();

        browser.sleep(500);

        p.catchregisterSuccessAlert(p.registerSuccessAlert);

        // // Check that registration form element is NOT displayed on page
        expect(p.firstNameInput.isPresent()).toBe(false);
    });


    it("should not allow to register without email, password, I agree", function () {
        p.getByLink();
        expect("test").toBe("written");
    });
    it("should not allow to register without email, password, I agree", function () {
        p.getByLink();
        expect("test").toBe("written");
    });
    it("should not allow to register without email, password, I agree", function () {
        p.getByLink();
        expect("test").toBe("written");
    });
    it("should not allow to register without email, password, I agree", function () {
        p.getByLink();
        expect("test").toBe("written");
    });

    it("should not allow to register with email in non-email format", function () {
        p.getByLink();
        expect("test").toBe("written");
    });

    it("should not allow to register with password with cyrillic symbols", function () {
        p.getByLink();
        expect("test").toBe("written");
    });
    it("should not allow to register with password with tm symbols", function () {
        p.getByLink();
        expect("test").toBe("written");
    });
    
    it("should show warnings about password strength", function () {
        p.getByLink();
        expect("test").toBe("written");
    });

    it("should show warnings about password strength", function () {
        p.getByLink();
        expect("test").toBe("written");
    });

    it("should open Terms and conditions in a new page", function () {
        p.getByLink();
        expect("test").toBe("written");
    });
    it("should actally show Terms and conditions", function () {
        p.getByLink();
        expect("test").toBe("written");
    });

    it("should not allow registration with existing email and show error", function () {
        p.getByLink();
        expect("test").toBe("written");
    });
});