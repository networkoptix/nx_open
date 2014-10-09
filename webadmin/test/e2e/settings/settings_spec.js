'use strict';

var AngularHomepage = require('./settings.po.js');
describe('Settings Page', function () {
    var p = new AngularHomepage();

    it('should set error class and make span visible if passwords differ', function () {
        p.get();

        // Passwords are empty => No error, No message
        expect(p.confirmPasswordInput.getAttribute('class')).toNotMatch('ng-invalid');
        expect(p.mustBeEqualSpan.getAttribute('class')).toMatch('ng-hide');

        // Password field does has value, confirm does not => => No error, No message
        p.passwordInput.sendKeys('p123');
        expect(p.confirmPasswordInput.getAttribute('class')).toNotMatch('ng-invalid');
        expect(p.mustBeEqualSpan.getAttribute('class')).toMatch('ng-hide');

        // Confirm has value and it differs from password
        p.confirmPasswordInput.sendKeys('q');
        expect(p.confirmPasswordInput.getAttribute('class')).toMatch('ng-invalid');
        expect(p.mustBeEqualSpan.getAttribute('class')).toNotMatch('ng-hide');

        // Clear confirm field. No error, no message
        p.confirmPasswordInput.clear();
        expect(p.confirmPasswordInput.getAttribute('class')).toNotMatch('ng-invalid');
        expect(p.mustBeEqualSpan.getAttribute('class')).toMatch('ng-hide');

        // Set values to be equal. => No error, no message
        p.confirmPasswordInput.sendKeys('p12');
        expect(p.confirmPasswordInput.getAttribute('class')).toMatch('ng-invalid');
        expect(p.mustBeEqualSpan.getAttribute('class')).toNotMatch('ng-hide');

        // Append '3' to make fields equal
        p.confirmPasswordInput.sendKeys('3');
        expect(p.confirmPasswordInput.getAttribute('class')).toNotMatch('ng-invalid');
        expect(p.mustBeEqualSpan.getAttribute('class')).toMatch('ng-hide');
    });
});
