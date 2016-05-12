'use strict';
var RegisterPage = require('./po.js');
describe('Confirmation email', function () {
    var p = new RegisterPage();

    beforeEach(function() {
        p.helper.get(p.helper.urls.restore_password);
    });

    it("can be sent again", function () {
        expect('functionality').toBe('returned');
        p.getRestorePassLink(p.helper.userEmail).then(function(url) {
            p.helper.get(url);
            p.setNewPassword(p.helper.userPassword);
            p.verifySecondAttemptFails(url, p.helper.userPassword);
        });
    });

    it("link works and does not log out user, if he was logged in", function () {
        p.getRestorePassLink(p.helper.userEmail).then(function(url) {
            p.helper.login(p.helper.userEmail2);
            p.helper.get(url);
            p.setNewPassword(p.helper.userPassword);
            expect(p.helper.forms.logout.dropdownToggle.isPresent()).toBe(true);
            expect(p.helper.forms.logout.dropdownToggle.getText()).toContain(p.helper.userEmail);
        });
    });

    xit("link redirects logged in user", function () {
        expect('test case').toBe('clear');
    });

    it("link has Login link, which opens login form", function () {
        p.getRestorePassLink(p.helper.userEmail).then(function(url) {
            p.helper.get(url);
            p.setNewPassword(p.helper.userPassword);
            p.messageLoginLink.click();
            expect(p.helper.forms.login.dialog.isDisplayed()).toBe(true);
        });
    });

});
