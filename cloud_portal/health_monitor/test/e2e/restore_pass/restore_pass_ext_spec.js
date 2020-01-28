'use strict';
var RestorePasswordPage = require('./po.js');
describe('Restore password page', function () {

    var p = new RestorePasswordPage();

    beforeEach(function() {
        p.helper.get(p.helper.urls.restore_password);
    });

    p.passwordField.check(function(){
        var deferred = protractor.promise.defer();
        p.getRestorePassLink(p.helper.userEmail).then(function(url){
            p.helper.get(url);
            browser.sleep(5000);
            deferred.fulfill();
        });
        return deferred.promise;
    }, p);


    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();
        p.emailInput.clear();
        p.emailInput.sendKeys(p.helper.userEmailWrong);
        p.submitButton.click();
        deferred.fulfill();
        return deferred.promise;
    }, p.alert.alertMessages.restorePassWrongEmail, p.alert.alertTypes.danger, false);

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();
        p.getRestorePassLink(p.helper.userEmail).then( function(url) {
            p.helper.get(url);
            p.setNewPassword(p.helper.userPassword);
            p.verifySecondAttemptFails(url, p.helper.userPassword);
            deferred.fulfill();
        });
        return deferred.promise;
    }, p.alert.alertMessages.restorePassWrongCode, p.alert.alertTypes.danger, false);

});