'use strict';

var PasswordFieldSuite = function () {
    var Helper = require('./helper.js');
    this.helper = new Helper();

    var self = this;

    this.passwordWeak = { class:'label-danger', text:'too short', title: 'Password must contain at least 6 characters' };
    this.passwordIncorrect = { class:'label-danger', text:'incorrect', title: 'Use only latin letters, numbers and keyboard symbols' };
    this.passwordCommon = { class:'label-danger', text:'too common', title: 'This password is in top most popular passwords in the world' };
    this.passwordFair = { class:'label-warning', text:'fair', title: 'Use numbers, symbols in different case and special symbols to make your password stronger' };
    this.passwordGood = { class:'label-success', text:'good', title: '' };

    this.invalidClass = 'ng-invalid';

    this.fieldGroup = element(by.css('password-input'));
    this.helpBlock = this.fieldGroup.element(by.css('.help-block'));
    this.makeVisible = this.fieldGroup.element(by.css('span.input-group-addon'));
    this.visibleInput = this.fieldGroup.element(by.css('input[type=text]'));

    this.check = function(getToPassword, pageObj){

        var checkPasswordInvalid = function (invalidClass) {
            expect(pageObj.passCheck.input.getAttribute('class')).toContain(invalidClass);
            expect(self.fieldGroup.$('.form-group').getAttribute('class')).toContain('has-error');
        };

        var checkPasswordWarning = function (strength) {
            expect(self.helpBlock.element(by.css('.label')).getAttribute('class')).toContain(strength.class);
            expect(self.helpBlock.element(by.css('.label')).getText()).toContain(strength.text);
        };

        // Check that particular message does NOT appear
        var checkNoPasswordWarning = function (strength) {
            expect(self.helpBlock.element(by.css('.label')).getText()).not.toContain(strength.text);
        };

        var notAllowPasswordWith = function (password) {
            pageObj.passCheck.input.sendKeys(password);
            pageObj.passCheck.submit.click();
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
                pageObj.passCheck.input.sendKeys('qwe');
                checkPasswordWarning(self.passwordWeak);

                pageObj.passCheck.input.clear();
                pageObj.passCheck.input.sendKeys(self.helper.userPasswordCyrillic);
                checkPasswordWarning(self.passwordIncorrect);

                pageObj.passCheck.input.clear();
                pageObj.passCheck.input.sendKeys('qwerty');
                checkPasswordWarning(self.passwordCommon);
                pageObj.passCheck.input.clear();
                pageObj.passCheck.input.sendKeys('password');
                checkPasswordWarning(self.passwordCommon);
                pageObj.passCheck.input.clear();
                pageObj.passCheck.input.sendKeys('12345678');
                checkPasswordWarning(self.passwordCommon);

                pageObj.passCheck.input.clear();
                pageObj.passCheck.input.sendKeys('asdoiu');
                checkPasswordWarning(self.passwordFair);

                pageObj.passCheck.input.clear();
                pageObj.passCheck.input.sendKeys('asdoiu2Q#');
                checkPasswordWarning(self.passwordGood);
            });
        });

        it("should show only 1 warning about strength at a time", function () {
            getToPassword().then(function(){
                pageObj.passCheck.input.sendKeys('123qwe');
                checkPasswordWarning(self.passwordCommon);

                pageObj.passCheck.input.sendKeys(protractor.Key.BACK_SPACE);
                checkNoPasswordWarning(self.passwordCommon);
                checkPasswordWarning(self.passwordWeak);
            });
        });

        it("should not allow password with leading or trailing spaces", function () {
            getToPassword().then(function(){
                pageObj.passCheck.input.sendKeys(' 123qwe');
                checkPasswordWarning(self.passwordIncorrect);

                pageObj.passCheck.input.clear();
                pageObj.passCheck.input.sendKeys('123qwe ');
                checkPasswordWarning(self.passwordIncorrect);
            });
        });

        it("password can be made visible", function() {
            getToPassword().then(function(){
                pageObj.passCheck.input.sendKeys('123qwe');
                self.makeVisible.click();
                expect(self.visibleInput.getAttribute('value')).toContain('123qwe');
            });
        });

        xit("symbols should be allowed in password", function() {
        });

        it("copy and paste into password field", function() {
            getToPassword().then(function(){
                self.makeVisible.click();
                self.visibleInput.sendKeys('passwordToCopy');
                self.visibleInput.sendKeys(protractor.Key.CONTROL, "a");
                self.visibleInput.sendKeys(protractor.Key.chord(protractor.Key.CONTROL, "c"));
                self.visibleInput.clear();
                expect(self.visibleInput.getAttribute('value')).not.toContain('passwordToCopy');
                self.visibleInput.sendKeys(protractor.Key.chord(protractor.Key.CONTROL, "v"));
                expect(self.visibleInput.getAttribute('value')).toContain('passwordToCopy');
            });
        });
    }
};

module.exports = PasswordFieldSuite;