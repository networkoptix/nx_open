'use strict';

var SettingsPage = require('./po.js');
describe('Cloud disconnect', function () {

    var p = new SettingsPage();

    beforeAll(function() {
        p.get();
        // if button Disconnect from Cloud is not present (system is not in Cloud),
        // then connect to Cloud
        p.disconnectFromCloudButton.isPresent().then( function(isPresent) {
            if( !isPresent ) {
                p.connectToCloud();
            }
        });
    });

    beforeEach(function(){
        p.disconnectFromCloudButton.click();
    });

    afterEach(function(){
        p.cloudDialogCloseButton.click();
    });

    afterAll(function() {
        p.disconnectFromCloud();
    });

    it("System is in cloud - we see cloud account and Disconnect button ",function(){
        p.cloudDialogCloseButton.click();
        expect(p.cloudSection.getText()).toContain('This system is linked to Nx Cloud account');
        p.disconnectFromCloudButton.click();
    });

    it("dialog: email input is locked",function(){
        expect(p.cloudEmailInput.getAttribute('readonly')).toBe('true');
    });

    it("dialog: wrong password triggers error message",function(){
        p.cloudPasswordInput.sendKeys('wrongpassword');
        p.dialogDisconnectButton.click();
        expect(p.dialogMessage.getText()).toContain('Login or password are incorrect');
    });

    it("dialog: button Disconnect system is disabled while password field is empty",function(){
        p.cloudPasswordInput.clear();
        expect(p.dialogDisconnectButton.isEnabled()).toBe(false);

        p.cloudPasswordInput.sendKeys(p.password);
        expect(p.dialogDisconnectButton.isEnabled()).toBe(true);
    });

    it("dialog: empty password field is highlighted with red",function(){
        p.cloudPasswordInput.clear()
            .sendKeys(protractor.Key.TAB); // blur
        expect(p.cloudPasswordInput.element(by.xpath('..')).getAttribute('class')).toContain('has-error');
    });

    it("dialog: can be closed with close button",function(){
        p.cloudDialogCloseButton.click();
        expect(p.dialog.isPresent()).toBe(false);
        p.disconnectFromCloudButton.click();
    });

    it("dialog: can be closed with cancel button",function(){
        p.dialogCancelButton.click();
        expect(p.dialog.isPresent()).toBe(false);
        p.disconnectFromCloudButton.click();
    });
});