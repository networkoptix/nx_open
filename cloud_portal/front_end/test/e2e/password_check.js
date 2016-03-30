'use strict';

var PasswordFieldSuite = function () {
    var Helper = require('./helper.js');
    this.helper = new Helper();

    var self = this;

    self.passwordWeak = { class:'label-danger', text:'too short', title: 'Password must contain at least 6 characters' };
    self.passwordIncorrect = { class:'label-danger', text:'incorrect', title: 'Use only latin letters, numbers and keyboard symbols' };
    self.passwordCommon = { class:'label-danger', text:'too common', title: 'This password is in top most popular passwords in the world' };
    self.passwordFair = { class:'label-warning', text:'fair', title: 'Use numbers, symbols in different case and special symbols to make your password stronger' };
    self.passwordGood = { class:'label-success', text:'good', title: '' };

    self.invalidClass = 'ng-invalid';
    // self.invalidClassRequired = 'ng-invalid-required';
    // self.invalidClassExists = 'ng-invalid-already-exists';

    this.check = function(getToPassword, pageObj){

        var checkPasswordInvalid = function (invalidClass) {
            expect(pageObj.passwordInput.getAttribute('class')).toContain(invalidClass);
            expect(pageObj.passwordGroup.$('.form-group').getAttribute('class')).toContain('has-error');
        };

        var checkPasswordWarning = function (strength) {
            expect(pageObj.passwordControlContainer.element(by.css('.label')).getAttribute('class')).toContain(strength.class);
            expect(pageObj.passwordControlContainer.element(by.css('.label')).getText()).toContain(strength.text);
        };

        // Check that particular message does NOT appear
        var checkNoPasswordWarning = function (strength) {
            expect(pageObj.passwordControlContainer.element(by.css('.label')).getAttribute('class')).not.toContain(strength.class);
            expect(pageObj.passwordControlContainer.element(by.css('.label')).getText()).not.toContain(strength.text);
        };

        var notAllowPasswordWith = function (password) {
            pageObj.passwordInput.sendKeys(password);
            pageObj.savePasswordButton.click();
            checkPasswordInvalid(self.invalidClass);
        };

        it("should not allow password with cyrillic symbols", function () {
            getToPassword().then(function(){
                notAllowPasswordWith(self.helper.userPasswordCyrillic);
            });
        });

        it("should not allow password with smile symbols", function () {
            getToPassword().then(function(){
                notAllowPasswordWith(self.helper.userPasswordSmile);
            });
        });

        it("should not allow password with hieroglyph symbols", function () {
            getToPassword().then(function(){
                notAllowPasswordWith(self.helper.userPasswordHierog);
            });
        });

        xit("should not allow password with tm symbols", function () {
            getToPassword().then(function(){
                notAllowPasswordWith(self.helper.userPasswordTm);
            });
        });

        it("should show warnings about password strength", function () {
            getToPassword().then(function(){
                pageObj.passwordInput.sendKeys('qwe');
                checkPasswordWarning(self.passwordWeak);

                pageObj.passwordInput.clear();
                pageObj.passwordInput.sendKeys('asdo iu2Q#');
                checkPasswordWarning(self.passwordIncorrect);

                pageObj.passwordInput.clear();
                pageObj.passwordInput.sendKeys('qwerty');
                checkPasswordWarning(self.passwordCommon);
                pageObj.passwordInput.clear();
                pageObj.passwordInput.sendKeys('password');
                checkPasswordWarning(self.passwordCommon);
                pageObj.passwordInput.clear();
                pageObj.passwordInput.sendKeys('12345678');
                checkPasswordWarning(self.passwordCommon);

                pageObj.passwordInput.clear();
                pageObj.passwordInput.sendKeys('asdoiu');
                checkPasswordWarning(self.passwordFair);

                pageObj.passwordInput.clear();
                pageObj.passwordInput.sendKeys('asdoiu2Q#');
                checkPasswordWarning(self.passwordGood);
            });
        });

        it("should show only 1 warning about strength at a time", function () {
            getToPassword().then(function(){
                pageObj.passwordInput.sendKeys('123qwe');
                checkPasswordWarning(self.passwordCommon);

                pageObj.passwordInput.sendKeys(protractor.Key.BACK_SPACE);
                checkNoPasswordWarning(self.passwordCommon);
                checkPasswordWarning(self.passwordWeak);
            });
        });

        it("should not allow pasword with leading or trailing spaces", function () {
            getToPassword().then(function(){
                pageObj.passwordInput.sendKeys(' 123qwe');
                checkPasswordWarning(self.passwordIncorrect);

                pageObj.passwordInput.clear();
                pageObj.passwordInput.sendKeys('123qwe ');
                checkPasswordWarning(self.passwordIncorrect);
            });
        });
    }
};

module.exports = PasswordFieldSuite;