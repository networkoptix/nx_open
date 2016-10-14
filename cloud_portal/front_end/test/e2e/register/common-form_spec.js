'use strict';
var RegisterPage = require('./po.js');
describe('Common form behaviour:', function () {
    var p = new RegisterPage();

    beforeEach(function() {
        p.helper.get(p.url);
    });

    it("focus first error field after submit", function () {
        p.submitButton.click();
        p.helper.checkElementFocusedBy(p.firstNameInput, 'id');
    });

    it("hide error on changing value", function () {
        p.submitButton.click();
        p.checkInputInvalid(p.firstNameInput, p.invalidClassRequired);
        p.firstNameInput.sendKeys('qwerty');
        expect(p.fieldWrap(p.firstNameInput).getAttribute('class')).not.toContain('has-error');
    });

    it("errors have messages (above form or to the right from input field)", function () {
        p.submitButton.click();
        browser.waitForAngular();
        expect(p.fieldWrap(p.firstNameInput).element(by.css('.help-block')).getText()).toContain('First name is required');
        expect(p.fieldWrap(p.lastNameInput).element(by.css('.help-block')).getText()).toContain('Last name is required');
        expect(p.fieldWrap(p.emailInput).element(by.css('.help-block')).getText()).toContain('Email is required');
        expect(p.passwordGroup.element(by.css('.help-block')).getText()).toContain('Password is required');
    });
});
