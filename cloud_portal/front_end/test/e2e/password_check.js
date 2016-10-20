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

    this.invalidClass = ' ng-invalid ';
    this.validClass = ' ng-valid ';

    this.fieldGroup = element(by.css('password-input'));
    this.helpBlock = this.fieldGroup.element(by.css('.help-block'));
    this.makeVisible = this.fieldGroup.element(by.css('span.input-group-addon'));
    this.maskedInput = this.fieldGroup.element(by.css('input[type=password]'));
    this.visibleInput = this.fieldGroup.element(by.css('input[type=text]'));

    this.check = function(getToPassword, pageObj){

        var checkPasswordInvalid = function (invalidClass) {
            expect(self.maskedInput.getAttribute('class')).toContain(invalidClass);
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
            self.maskedInput.sendKeys(password);
            pageObj.passCheck.submit.click();
            checkPasswordInvalid(self.invalidClass);
        };

        it("password field rejects password with cyrillic symbols", function () {
            getToPassword().then(function(){
                notAllowPasswordWith(self.helper.userPasswordCyrillic);
            });
        });

        it("password field rejects password with smile symbols", function () {
            getToPassword().then(function(){
                notAllowPasswordWith(self.helper.userPasswordSmile);
            });
        });

        it("password field rejects password with hieroglyph symbols", function () {
            getToPassword().then(function(){
                notAllowPasswordWith(self.helper.userPasswordHierog);
            });
        });

        it("password field rejects password with tm symbols", function () {
            getToPassword().then(function(){
                notAllowPasswordWith(self.helper.userPasswordTm);
            });
        });

        it("password field shows warnings about password strength", function () {
            getToPassword().then(function(){
                self.maskedInput.sendKeys('qwe');
                checkPasswordWarning(self.passwordWeak);

                self.maskedInput.clear()
                    .sendKeys(self.helper.userPasswordCyrillic);
                checkPasswordWarning(self.passwordIncorrect);

                self.maskedInput.clear()
                    .sendKeys('qwertyui');
                checkPasswordWarning(self.passwordCommon);
                self.maskedInput.clear()
                    .sendKeys('password');
                checkPasswordWarning(self.passwordCommon);
                self.maskedInput.clear()
                    .sendKeys('12345678');
                checkPasswordWarning(self.passwordCommon);

                self.maskedInput.clear()
                    .sendKeys('asdoiurFf');
                checkPasswordWarning(self.passwordFair);

                self.maskedInput.clear()
                    .sendKeys('asdoiu2Q#');
                checkPasswordWarning(self.passwordGood);
            });
        });

        it("password field shows only 1 warning about strength at a time", function () {
            getToPassword().then(function(){
                self.maskedInput.sendKeys('12345678');
                checkPasswordWarning(self.passwordCommon);

                self.maskedInput.sendKeys(protractor.Key.BACK_SPACE);
                checkNoPasswordWarning(self.passwordCommon);
                checkPasswordWarning(self.passwordWeak);
            });
        });

        it("password field rejects password with leading or trailing spaces", function () {
            getToPassword().then(function(){
                self.maskedInput.sendKeys(' 123qwe');
                checkPasswordWarning(self.passwordIncorrect);

                self.maskedInput.clear()
                    .sendKeys('123qwe ');
                checkPasswordWarning(self.passwordIncorrect);
            });
        });

        it("password field input can be made visible", function() {
            getToPassword().then(function(){
                self.maskedInput.sendKeys('visiblePassword');
                self.makeVisible.click();
                expect(self.visibleInput.getAttribute('value')).toContain('visiblePassword');
            });
        });

        it("password field allows copy and paste visible input", function() {
            getToPassword().then(function(){
                self.makeVisible.click(); // make password input human-readable
                self.visibleInput.sendKeys('passwordToCopy');

                // Copy + paste
                self.visibleInput.sendKeys(protractor.Key.CONTROL, "a")
                    .sendKeys(protractor.Key.CONTROL, "c")
                    .clear();
                expect(self.visibleInput.getAttribute('value')).not.toContain('passwordToCopy');
                self.visibleInput.sendKeys(protractor.Key.CONTROL, "v");
                expect(self.visibleInput.getAttribute('value')).toContain('passwordToCopy');

                // Cut + paste
                self.visibleInput.sendKeys(protractor.Key.CONTROL, "a")
                    .sendKeys(protractor.Key.CONTROL, "x");
                expect(self.visibleInput.getAttribute('value')).not.toContain('passwordToCopy');
                self.visibleInput.sendKeys(protractor.Key.CONTROL, "v");
                expect(self.visibleInput.getAttribute('value')).toContain('passwordToCopy');
            });
        });

        it("password field allows copy visible and paste dotted input", function() {
            getToPassword().then(function(){
                self.makeVisible.click(); // make password input human-readable
                self.visibleInput.sendKeys('passwordToCopy')
                    .sendKeys(protractor.Key.CONTROL, "a")
                    .sendKeys(protractor.Key.CONTROL, "c")
                    .clear();
                self.makeVisible.click(); // make password input value replaced with dots
                self.maskedInput.sendKeys(protractor.Key.CONTROL, "v");
                self.makeVisible.click(); // make password input human-readable
                expect(self.visibleInput.getAttribute('value')).toContain('passwordToCopy');
            });
        });

        it("password field allows password with symbols", function() {
            getToPassword().then(function(){
                self.maskedInput.sendKeys(self.helper.userPasswordSymb);
                expect(self.maskedInput.getAttribute('value')).toContain(self.helper.userPasswordSymb);
                expect(self.maskedInput.getAttribute('class')).toContain(self.validClass);
                expect(self.fieldGroup.$('.form-group').getAttribute('class')).not.toContain('has-error');
                pageObj.passCheck.submit.click();
            });
        });
    }
};

module.exports = PasswordFieldSuite;