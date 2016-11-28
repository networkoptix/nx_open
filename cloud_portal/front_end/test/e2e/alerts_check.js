'use strict';

var AlertSuite = function () {
    var self = this;
    
    this.alertTypes = {danger: 'danger', success: 'success'};
    this.alertMessages = {
        loginIncorrect: 'Login or password are incorrect',
        loginNotActive: 'Your account wasn\'t confirmed yet',
        registerSuccess: 'The account was successfully registered. We\'ve sent a confirmation message to your email address.\nOpen it up to activate the account.',
        registerConfirmSuccess: 'Your account has been successfully activated.',
        registerConfirmError: 'Couldn\'t activate your account: Wrong confirmation code',
        accountSuccess: 'Your account was successfully saved.',
        restorePassWrongEmail: 'Couldn\'t send confirmation email: Email isn\'t registered in portal',
        restorePassConfirmSent: 'We\'ve sent a password reset message to your email address. Open it up to reset the password.',
        restorePassWrongCode: 'Couldn\'t save new password: Wrong confirmation code',
        restorePassSuccess: 'Password successfully saved',
        changePassWrongCurrent: 'Couldn\'t change your password: Current password is incorrect.',
        changePassSuccess: 'Your password was successfully changed.',
        systemAccessError: 'System info is unavailable: Some unexpected error has happened',
        systemAccessRestricted: 'System info is unavailable: You have no access to this system',
        permissionDeleteSuccess: 'Permissions were removed from ',
        permissionAddSuccess: 'New permissions saved'
    };

    this.submitButton = element(by.css('process-button')).element(by.css('button'));
    this.alert = element(by.css('.ng-toast__message')).element(by.css('.alert'));
    this.alertCloseButton = this.alert.element(by.css('button.close'));
    this.successMessageElem = element(by.css('.process-success'));

    function waitAlert(timeout){
        var waitMs = timeout || 1700;
        browser.sleep(waitMs);
        browser.ignoreSynchronization = true;
        expect(self.alert.isDisplayed()).toBe(true);
    }

    function checkAlertContent(message, type){
        expect(self.alert.getText()).toContain(message);
        expect(self.alert.getAttribute('class')).toContain(type);
    }

    function closeAlert(shouldCloseOnTimeout){
        // Alerts that have close button do not close by clicking on alert.
        // Thus, the following code decides, how to close it
        if (!shouldCloseOnTimeout) {
            self.alertCloseButton.click().then(function() {console.log('ALERT CLOSED clicked close button')});}
        else {
            self.alert.click();}
        browser.ignoreSynchronization = false;
        browser.sleep(300);
        expect(self.alert.isPresent()).toBe(false);
    }

    function closeAlertByButton(){
        self.alertCloseButton.click();
        browser.ignoreSynchronization = false;
        browser.sleep(300);

        expect(self.alert.isPresent()).toBe(false);
    }

    function finishAlertCheck() {
        browser.ignoreSynchronization = false;
    }

    function checkAlertTimeout(shouldCloseOnTimeout){
        browser.sleep(1000);
        expect(self.alert.isPresent()).toBe(true);

        browser.sleep(4000);
        expect(self.alert.isPresent()).toBe(!shouldCloseOnTimeout);
    }

    this.catchAlert = function (message, type, timeout) {
        waitAlert(timeout);
        checkAlertContent(message, type);
        finishAlertCheck();
    };

    this.checkAlert = function (callAlert, message, type, shouldCloseOnTimeout){

        it("should show error alert",function(){
            callAlert().then(function(){
                waitAlert();
                checkAlertContent(message, type);
                closeAlert(shouldCloseOnTimeout);
                //finishAlertCheck();
            });
        });

        if(shouldCloseOnTimeout){
            it("should close on timeout",function(){
                callAlert().then(function(){
                    waitAlert();
                    checkAlertTimeout(true);
                    finishAlertCheck();
                });
            });
        } 
        else{
            it("should not close on timeout",function(){
                callAlert().then(function(){
                    waitAlert();
                    checkAlertTimeout(false);
                    closeAlertByButton();
                    finishAlertCheck();
                });
            });
        }
    }
};

module.exports = AlertSuite;
