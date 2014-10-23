'use strict';

var SettingsPage = require('./po.js');
describe('Settings Page', function () {

    //it("should stop test",function(){expect("other test").toBe("uncommented");});return;

    var p = new SettingsPage();
    var ptor = protractor.getInstance();


    it('Change password: should check oldpassword and newpassword and show some info', function () {
        p.get();

        var confirmContainer = p.confirmPasswordInput.element(by.xpath('../..'));
        p.oldPasswordInput.sendKeys('1'); // Set something to old password field

        // Passwords are empty => No error, No message, No button
        expect(p.confirmPasswordInput.getAttribute('class')).toMatch('ng-invalid');
        expect(confirmContainer.getAttribute('class')).toNotMatch('has-error');
        expect(p.mustBeEqualSpan.getAttribute('class')).toMatch('ng-hide');
        expect(p.changePasswordButton.isEnabled()).toBe(false);

        // Password field does has value, confirm does not => => No error, No message
        p.passwordInput.sendKeys('p123');
        expect(p.confirmPasswordInput.getAttribute('class')).toMatch('ng-invalid');
        expect(confirmContainer.getAttribute('class')).toNotMatch('has-error');
        expect(p.mustBeEqualSpan.getAttribute('class')).toMatch('ng-hide');
        expect(p.changePasswordButton.isEnabled()).toBe(false);

        // Confirm has value and it differs from password
        p.confirmPasswordInput.sendKeys('q');
        expect(p.confirmPasswordInput.getAttribute('class')).toMatch('ng-invalid');
        expect(confirmContainer.getAttribute('class')).toMatch('has-error');
        expect(p.mustBeEqualSpan.getAttribute('class')).toNotMatch('ng-hide');
        expect(p.changePasswordButton.isEnabled()).toBe(false);

        // Clear confirm field. No error, no message
        p.confirmPasswordInput.clear();
        expect(p.confirmPasswordInput.getAttribute('class')).toMatch('ng-invalid');
        expect(confirmContainer.getAttribute('class')).toNotMatch('has-error');
        expect(p.mustBeEqualSpan.getAttribute('class')).toMatch('ng-hide');
        expect(p.changePasswordButton.isEnabled()).toBe(false);

        // Set values to be equal. => No error, no message
        p.confirmPasswordInput.sendKeys('p12');
        expect(p.confirmPasswordInput.getAttribute('class')).toMatch('ng-invalid');
        expect(confirmContainer.getAttribute('class')).toMatch('has-error');
        expect(p.mustBeEqualSpan.getAttribute('class')).toNotMatch('ng-hide');
        expect(p.changePasswordButton.isEnabled()).toBe(false);

        // Append '3' to make fields equal
        p.confirmPasswordInput.sendKeys('3');
        expect(p.confirmPasswordInput.getAttribute('class')).toNotMatch('ng-invalid');
        expect(confirmContainer.getAttribute('class')).toNotMatch('has-error');
        expect(p.mustBeEqualSpan.getAttribute('class')).toMatch('ng-hide');
        expect(p.changePasswordButton.isEnabled()).toBe(true);
    });

    it('Change password: should not allow change password without an old password', function () {
        p.setPassword('p12'); // send something to passwords
        p.setConfirmPassword('p12');

        p.oldPasswordInput.clear();
        expect(p.changePasswordButton.isEnabled()).toBe(false);

        p.oldPasswordInput.sendKeys('1');
        expect(p.changePasswordButton.isEnabled()).toBe(true);
    });

    it('Change password: should not change password if old password was wrong', function () {
        p.setPassword('1234'); // send something to passwords
        p.setConfirmPassword('1234');
        p.setOldPassword('wrong old password');

        expect(p.changePasswordButton.isEnabled()).toBe(true);
        p.changePasswordButton.click().then(function(){
            var alertDialog = ptor.switchTo().alert(); // Expect alert.then( // <- this fixes the problem
            expect(alertDialog.getText()).toContain("Error");
            alertDialog.accept();
        });
    });

    it('Change password: should change password normally', function () {
        expect("change password tests").toBe("written");
        return
        p.get();

        p.passwordInput.sendKeys('1234'); // send something to passwords
        p.confirmPasswordInput.sendKeys('1234');
        p.oldPasswordInput.sendKeys('123');

        /*p.changePasswordButton.click().then(function(){
            var alertDialog = ptor.switchTo().alert(); // Expect alert
            expect(alertDialog.getText()).toEqual("Settings changed");
        });*/
        //

        //restore password now
        p.passwordInput.clear();
        p.confirmPasswordInput.clear();
        p.oldPasswordInput.clear();

        p.passwordInput.sendKeys('123'); // send something to passwords
        p.confirmPasswordInput.sendKeys('123');
        p.oldPasswordInput.sendKeys('1234');

        //message "Password changed" is visible
    });

    it("Change port: should not allow to set empty or bad port",function(){
        p.get();

        p.setPort(100000);
        expect(p.portInput.getAttribute('class')).toMatch('ng-invalid');
        expect(p.saveButton.isEnabled()).toBe(false);

        p.setPort('');
        expect(p.portInput.getAttribute('class')).toMatch('ng-invalid');
        expect(p.saveButton.isEnabled()).toBe(false);

        p.setPort('port');
        expect(p.portInput.getAttribute('class')).toMatch('ng-invalid');
        expect(p.saveButton.isEnabled()).toBe(false);

        p.setPort(7000);
        expect(p.portInput.getAttribute('class')).toNotMatch('ng-invalid');
        expect(p.saveButton.isEnabled()).toBe(true);
    });


    it("Change port: should save port and show message about restart",function(){
        expect("actual changing port").toBe("tested manually");
    });



    it("Change system name: should not allow set empty name",function(){
        p.setSystemName('');
        expect(p.systemNameInput.getAttribute('class')).toMatch('ng-invalid');
        expect(p.saveButton.isEnabled()).toBe(false);

        p.setSystemName('some good name');
        expect(p.systemNameInput.getAttribute('class')).toNotMatch('ng-invalid');
        expect(p.saveButton.isEnabled()).toBe(true);
    });


    it("Change system name: should allow change name",function() {

        p.get();
        var oldSystemName = '';
        var temporarySystemName = 'some good name';
        p.systemNameInput.getAttribute('value').then(function(val){
            oldSystemName = val;
        });

        p.setSystemName(temporarySystemName);

        p.saveButton.click().then(function(){
            var alertDialog = ptor.switchTo().alert(); // Expect alert.then( // <- this fixes the problem
            expect(alertDialog.getText()).toEqual("Settings saved");
            alertDialog.accept();

            p.get();
            expect( p.systemNameInput.getAttribute('value')).toEqual(temporarySystemName);
            p.setSystemName(oldSystemName);

            p.saveButton.click().then(function(){
                var alertDialog = ptor.switchTo().alert(); // Expect alert.then( // <- this fixes the problem
                expect(alertDialog.getText()).toEqual("Settings saved");
                alertDialog.accept();
            });
        });
    });



    it("Link to another servers: should show links for servers",function(){
        expect(p.mediaServersLinks.count()).toBeGreaterThan(0); // There are some links, related to servers array
    });

    it("Link to another servers: should show current system in the list",function(){
        expect( p.activeMediaServerLink.isPresent()).toBe(true);
    });

    it("Link to another servers: should find linkable link for current server",function(){
        var current = p.activeMediaServerLink;

        //browser.wait(function() { return false; }, 30*1000); //30 seconds to ping all sites
        current.getAttribute("href").then(function(url){
            // Ping url
            var request = require("request");
            request({uri:url + "/api/moduleInformation"},function(error,response,body){
                expect (!error && response.statusCode == 200).toBe(true);
            });
        });
    });


    it("Restart: should show clickable button",function(){
        expect(p.resetButton.isEnabled()).toBe(true);
    });

    it("Merge: should show clickable button",function(){
        expect(p.mergeButton.isEnabled()).toBe(true);
    });

});