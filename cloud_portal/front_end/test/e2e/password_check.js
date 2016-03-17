'use strict';

var PasswordFieldSuite = function () {
    var self = this;
    
    self.userPasswordCyrillic = 'йцуфывячс';
    self.userPasswordSmile = '☠☿☂⊗⅓∠∩λ℘웃♞⊀☻★';
    self.userPasswordHierog = '您都可以享受源源不絕的好禮及優惠';

    self.passwordWeak = { class:'label-danger', text:'too short', title: 'Password must contain at least 6 characters' };
    self.passwordCommon = { class:'label-danger', text:'too common', title: 'This password is in top most popular passwords in the world' };
    self.passwordFair = { class:'label-warning', text:'fair', title: 'Use numbers, symbols in different case and special symbols to make your password stronger' };
    self.passwordGood = { class:'label-success', text:'good', title: '' };

    self.invalidClass = 'ng-invalid';
    // self.invalidClassRequired = 'ng-invalid-required';
    // self.invalidClassExists = 'ng-invalid-already-exists';



    this.check = function(pageObj, url){

        var checkPasswordInvalid = function (invalidClass) {
            expect(pageObj.passwordInput.getAttribute('class')).toContain(invalidClass);
            expect(pageObj.passwordGroup.$('.form-group').getAttribute('class')).toContain('has-error');
        }

        var checkPasswordWarning = function (strength) {
            expect(pageObj.passwordControlContainer.element(by.css('.label')).getAttribute('class')).toContain(strength.class);
            expect(pageObj.passwordControlContainer.element(by.css('.label')).getText()).toContain(strength.text);
        }

        var notAllowPasswordWith = function (password) {
            browser.get(url);

            pageObj.prepareToPasswordCheck();
            pageObj.passwordInput.sendKeys(password);

            pageObj.submitButton.click();
            checkPasswordInvalid(self.invalidClass);
        }

        it("should not allow password with cyrillic symbols", function () {
            notAllowPasswordWith(self.userPasswordCyrillic);
        });

        it("should not allow password with cyrillic symbols", function () {
            notAllowPasswordWith(self.userPasswordSmile);
        });

        it("should not allow password with cyrillic symbols", function () {
            notAllowPasswordWith(self.userPasswordHierog);
        });

        xit("should not allow password with tm symbols", function () {
            notAllowPasswordWith(self.userPasswordTm);
        });

        it("should show warnings about password strength", function () {
            browser.get(url);

            pageObj.passwordInput.sendKeys('qwe');
            checkPasswordWarning(self.passwordWeak);

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
    }
};


module.exports = PasswordFieldSuite;