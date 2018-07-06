'use strict';

var AlertSuite = function () {
    var self = this;
    
    this.alertTypes = {danger: 'danger', success: 'success'};
    this.alertMessages = {
        loginIncorrect: 'Incorrect Email or password',
        loginNotActive: 'This account is not activated yet. Send activation link again',
        registerSuccess: 'Account is created.\nConfirmation message is sent to',
        registerConfirmSuccess: 'Your account is successfully activated',
        registerConfirmError: 'Account is already activated or confirmation code is incorrect',
        accountSuccess: 'Your account is successfully saved',
        restorePassWrongEmail: 'Cannot send confirmation Email: Account does not exist',
        restorePassConfirmSent: 'Password reset instructions are sent to Email',
        restorePassWrongCode: 'Cannot save password: Confirmation code is already used or incorrect',
        restorePassSuccess: 'Password successfully saved',
        changePassWrongCurrent: 'Cannot save password: Current password is incorrect',
        changePassSuccess: 'Your account is successfully saved',
        systemAccessError: 'System info is unavailable: Some unexpected error has happened',
        systemAccessRestricted: 'Failed to access System\nThis link is broken,\n'+
        'System may be disconnected from Nx Cloud,\nAccess to this System may be revoked,\n'+
        'or you may be logged into a different account',
        permissionDeleteSuccess: 'Permissions were removed from ',
        permissionAddSuccess: 'New permissions saved',
        activationLinkSent: 'Activation Link Sent'
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
