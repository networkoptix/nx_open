'use strict';
var RegisterPage = require('./po.js');
describe('Confirmation email', function () {
    var p = new RegisterPage();

    beforeEach(function() {
        p.helper.get(p.helper.urls.restore_password);
    });

    it("link works and logs user out, if he was logged in", function () {
        p.getRestorePassLink(p.helper.userEmail).then(function(url) {
            p.helper.login(p.helper.userEmail2);
            p.helper.get(url);

            expect(p.helper.forms.logout.alreadyLoggedIn.isDisplayed()).toBe(true);
            p.helper.forms.logout.logOut.click(); // log out

            p.setNewPassword(p.helper.userPassword);
            p.helper.waitIfNotPresent(p.messageLoginLink, 500);
            expect(p.helper.forms.logout.dropdownToggle.isDisplayed()).toBe(false);
            expect(p.helper.forms.restorePassPassword.messageLoginLink.isDisplayed()).toBe(true);
            expect(p.helper.forms.login.openLink.isDisplayed()).toBe(true);
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
