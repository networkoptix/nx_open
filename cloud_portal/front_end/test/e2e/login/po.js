'use strict';

var LoginPage = function () {

    this.get = function () {
        browser.get('/');
        browser.waitForAngular();

        this.loginButton.click();
    };

    this.userEmail1 = 'ekorneeva+1@networkoptix.com';
    this.userEmail2 = 'ekorneeva+2@networkoptix.com';
    this.userEmailWrong = 'nonexistingperson@networkoptix.com';
    this.userPassword = 'qweasd123';
    this.userPasswordWrong = 'qweqwe123';

    this.loginButton = element(by.linkText('Login'));

    this.loginDialog = element(by.css('.modal-dialog'));

    this.emailInput = this.loginDialog.element(by.model('auth.email'));
    this.passwordInput = this.loginDialog.element(by.model('auth.password'));
    
    this.emailInputWrap = this.emailInput.element(by.xpath('..'));
    this.passwordInputWrap = this.passwordInput.element(by.xpath('..'));
    
    this.checkPasswordMissing = function() {
        expect(this.passwordInput.getAttribute('class')).toContain('ng-invalid-required');
        expect(this.passwordInputWrap.getAttribute('class')).toContain('has-error');
    }

    this.checkEmailMissing = function() {
        expect(this.emailInput.getAttribute('class')).toContain('ng-invalid-required');
        expect(this.emailInputWrap.getAttribute('class')).toContain('has-error');
    }

    this.checkEmailInvalid = function() {
        expect(this.emailInput.getAttribute('class')).toContain('ng-invalid');
        expect(this.emailInputWrap.getAttribute('class')).toContain('has-error');
    }

    this.dialogLoginButton = this.loginDialog.element(by.buttonText('Login'));
    this.dialogCloseButton = this.loginDialog.all(by.css('button.close')).first();

    this.loginIncorrectAlert = element(by.css('process-alert[process=login]')).element(by.css('.alert')); // alert with eror message
    this.catchLoginIncorrectAlert = function (loginIncorrectAlert) {
        // Workaround due to Protractor bug with timeouts https://github.com/angular/protractor/issues/169
        // taken from here http://stackoverflow.com/questions/25062748/testing-the-contents-of-a-temporary-element-with-protractor
        browser.sleep(500);
        browser.ignoreSynchronization = true;
        expect(loginIncorrectAlert.getText()).toContain('Login or password are incorrect');
        browser.sleep(500);
        browser.ignoreSynchronization = false;
    }

    this.loginSuccessElement = element.all(by.css('.auth-visible')).first(); // some element on page, that is only visible when user is authenticated

    this.loginDialogBackground = element(by.css('.modal')); // login dialog overlay

    this.navbar = element(by.css('header')).element(by.css('.navbar'));
    this.userAccountDropdownToggle = this.navbar.element(by.css('a[uib-dropdown-toggle]'));
    this.userAccountDropdownMenu = this.navbar.element(by.css('[uib-dropdown-menu]'));
    this.userLogoutLink = this.userAccountDropdownMenu.element(by.linkText('Logout'));

    this.rememberCheckbox = element(by.model('auth.remember'));
    this.iForgotPasswordLink = element(by.linkText('I forgot my password'));

    this.htmlBody = element(by.css('body'));

    this.restoreEmailInput = element(by.model('data.email'));

    this.login = function() {
        this.dialogLoginButton.click();
        browser.sleep(2000); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is displayed on page
        expect(this.loginSuccessElement.isDisplayed()).toBe(true);
    }
    this.logout = function() {
        expect(this.userAccountDropdownToggle.isDisplayed()).toBe(true);

        // this.userAccountDropdownToggle.isDisplayed().then(function(visible)
        // {
        //     if(visible){

        //         this.userAccountDropdownToggle.click();
        //         this.userLogoutLink.click();
        //         browser.sleep(500); // such a shame, but I can't solve it right now

        //         expect(this.loginSuccessElement.isDisplayed()).toBe(false);
        //     }
        // });

        this.userAccountDropdownToggle.click();
        this.userLogoutLink.click();
        browser.sleep(500); // such a shame, but I can't solve it right now

        expect(this.loginSuccessElement.isDisplayed()).toBe(false);

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(this.loginSuccessElement.isDisplayed()).toBe(false);
    }

    // this.wait = function(promise, timeout) {
    //     while(timePassed < timeout && !checker()){
    //         console.log("try" ,timePassed);
    //         browser.sleep(Math.min(timePassed, 200));
    //         timePassed += 200;
    //     }
    // }

    // this.waitDisplay = function(element,timeout) {
    //     return this.wait( function() {
    //         return element.isDisplayed();
    //     }, timeout);
    // }
};

module.exports = LoginPage;