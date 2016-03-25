'use strict';
var RestorePasswordPage = require('./po.js');
describe('Restore password suite', function () {

    var p = new RestorePasswordPage();

    p.alert.checkAlert(function(){
        p.get();
    }, p.alert.alertMessages.restorePassWrongEmail, p.alert.alertTypes.danger, true);

    it("should restore password with a code sent to an email", function () {
        var userEmail = p.userEmail1;
        var userPassword = p.userPassword;
        p.get();
        p.iForgotPasswordLink.click();
        p.restoreEmailInput.sendKeys(userEmail);
        p.submitButton.click();

        p.alert.catchAlert( p.alert.alertMessages.restorePassConfirmSent, p.alert.alertTypes.success);

        browser.controlFlow().wait(p.helper.getEmailTo(userEmail).then(function (email) {
            expect(email.subject).toContain("Restore your password");
            expect(email.headers.to).toEqual(userEmail);

            // extract registration token from the link in the email message
            var pattern = /\/static\/index.html#\/restore_password\/([\w=]+)/g;
            var regCode = pattern.exec(email.html)[1];
            console.log(regCode);
            browser.get('/#/restore_password/' + regCode);

            expect(p.newPasswordInput.isPresent()).toBe(true);
            p.newPasswordInput.sendKeys(userPassword); //should be new password here, but old one will do the job too
            p.savePasswordButton.click();
            p.alert.catchAlert( p.alert.alertMessages.restorePassSuccess, p.alert.alertTypes.success);
            browser.refresh();

            p.newPasswordInput.sendKeys(userPassword);
            p.savePasswordButton.click();
            p.alert.catchAlert( p.alert.alertMessages.restorePassWrongCode, p.alert.alertTypes.danger);

            p.helper.login(userEmail, userPassword);
            p.helper.logout();
        }));
    });

    xit("should remember email from login form", function () {
        expect("test").toBe("written");
    });

    xit("should demand that email field is not empty", function () {
        expect("test").toBe("written");
    });

    xit("should validate user email *@*.*", function () {
        expect("test").toBe("written");
    });

    xit("should not succeed, if email is not registered", function () {
        expect("test").toBe("written");
    });

    p.passwordField.check();

    xit("should be able to set new password (which is same as old)", function () {
        expect("test").toBe("written");
    });

    xit("should set new password, login with new password change password back", function () {
        expect("test").toBe("written"); // shall I keep current password in variable to avoid such stuff??
    });

    xit("should not allow to use one restore link twice", function () {
        expect("test").toBe("written");
    });

    xit("should allow not-activated user to restore password", function () {
        expect("test").toBe("written");
    });

    xit("should make not-activated user active by restoring password", function () {
        expect("test").toBe("written");
    });

    xit("should allow logged in user visit restore password page", function () {
        expect("test").toBe("written");
    });

    xit("should log user out if he visits restore password link from email", function () {
        expect("test").toBe("written");
    });
});