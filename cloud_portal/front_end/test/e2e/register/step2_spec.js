'use strict';
var RegisterPage = require('./po.js');
describe('Registration step2', function () {
    var p = new RegisterPage();

    it("should activate registration with a registration code sent to an email", function () {
        var userEmail = p.helper.register();

        p.getActivationPage(userEmail).then( function() {
            p.alert.catchAlert( p.alert.alertMessages.registerConfirmSuccess, p.alert.alertTypes.success);
            browser.refresh();
            p.alert.catchAlert( p.alert.alertMessages.registerConfirmError, p.alert.alertTypes.danger);

            p.helper.login(userEmail, p.helper.userPassword);
            p.helper.logout();
        });
    });

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();

        var userEmail = p.helper.register();

        p.getActivationPage(userEmail).then( function() {
            deferred.fulfill();
        });

        return deferred.promise;
    }, p.alert.alertMessages.registerConfirmSuccess, p.alert.alertTypes.success, false);

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();

        var userEmail = p.helper.register();

        p.getActivationPage(userEmail).then( function() {
            p.alert.catchAlert( p.alert.alertMessages.registerConfirmSuccess, p.alert.alertTypes.success);
            browser.refresh();

            deferred.fulfill();
        });

        return deferred.promise;
    }, p.alert.alertMessages.registerConfirmError, p.alert.alertTypes.danger, false);
});