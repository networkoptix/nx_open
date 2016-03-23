'use strict';

var AlertSuite = function () {
    var self = this;
    
    this.alertTypes = {danger: 'danger', success: 'success'};
    this.alertMessages = {
        loginDanger: 'Login or password are incorrect',
        registerSuccess: 'Your account was successfully registered. Please, check your email to confirm it',
        accountSuccess: 'Your account was successfully saved.',
        restorePassWrongEmail: '',
        restorePassConfirmSent: '',
        restorePassWrongCode: '',
        restorePassSuccess: '',
        changePassWrongCurrent: 'Couldn\'t change your password: Current password doesn\'t match',
        changePassSuccess: 'Your password was successfully changed.'
    };

    this.submitButton = element(by.css('process-button')).element(by.css('button'));
    this.alert = element(by.css('process-alert')).element(by.css('.alert'));
    this.alertCloseButton = this.alert.element(by.css('button.close'));

    function waitAlert(){
        browser.sleep(1000);
        browser.ignoreSynchronization = true;
        expect(self.alert.isDisplayed()).toBe(true);
    }

    function checkAlertContent(message, type){
        expect(self.alert.getText()).toContain(message);
        expect(self.alert.getAttribute('type')).toContain(type);
    }

    function closeAlert(){
        self.alertCloseButton.click();
        browser.sleep(1000);

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

    this.checkAlert = function (callAlert, message, type, shouldCloseOnTimeout){

        it("should show error alert and close it by button",function(){
            callAlert();
            waitAlert();
            checkAlertContent(message, type);
            closeAlert();
            finishAlertCheck();
        });

        
        if(shouldCloseOnTimeout){
            it("should close on timeout",function(){
                callAlert();
                waitAlert();
                checkAlertTimeout(true);
                finishAlertCheck();
            });
        } 
        else{
            it("should not close on timeout",function(){
                callAlert();
                waitAlert();
                checkAlertTimeout(false);
                closeAlert();
                finishAlertCheck();
            });
        }
    }
};

module.exports = AlertSuite;
