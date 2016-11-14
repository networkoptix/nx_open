'use strict';

var SettingsPage = require('./po.js');
describe('Cloud disconnect', function () {

    var p = new SettingsPage();

    beforeAll(function() {
        p.get();
        p.helper.getTab("System").click();
    });

    beforeEach(function(){
        p.helper.getTab("System").click();
        // if button Disconnect from Cloud is not present (system is not in Cloud),
        // then connect to Cloud

        p.helper.checkPresent(p.disconnectFromCloudButton).then(
            p.disconnectFromCloudButton.click(),
            function() {
                console.log('Disconnect from Cloud Button not found, so logging into Cloud');
                p.connectToCloud();
                p.getSysTab(); // to end all pending requests
                p.disconnectFromCloudButton.click();
            });
    });

    afterEach(function(){
        // if dialog is opened, close it
        p.helper.checkPresent(p.dialogCancelButton).then(function () {
            p.dialogCancelButton.click();
        }, function() {}); // skip if absent
    });

    afterAll(function() {
        p.disconnectFromCloud();
    });

    it("System is in cloud - we see cloud account and Disconnect button ",function(){
        p.dialogCancelButton.click();
        expect(p.cloudSection.getText()).toContain('This system is linked to Nx Cloud');
        expect(p.goToCloudAccButton.getAttribute('href')).toContain('https://');
        expect(p.goToCloudAccButton.getAttribute('href')).toContain('?from=webadmin&context=settings');
    });

    it("dialog: wrong password triggers error message",function(){
        p.oldCloudPasswordInput.sendKeys('wrongpassword');
        p.dialogDisconnectButton.click();
        expect(p.dialog.getText()).toContain('Wrong password');
        p.dialogButtonClose.click();
    });

    it("dialog: button Disconnect system is disabled while password field is empty",function(){
        p.oldCloudPasswordInput.clear();
        expect(p.dialogDisconnectButton.isEnabled()).toBe(false);

        p.oldCloudPasswordInput.sendKeys(p.password);
        expect(p.dialogDisconnectButton.isEnabled()).toBe(true);
    });

    // Fails, because empty field is not highlighted
    it("dialog: empty password field is highlighted with red",function(){
        p.oldCloudPasswordInput.clear()
            .sendKeys(protractor.Key.TAB); // blur
        expect(p.oldCloudPasswordInput.element(by.xpath('..')).getAttribute('class')).toContain('has-error');
    });

    it("dialog: can be closed with cancel button",function(){
        p.dialogCancelButton.click();
        expect(p.dialog.isPresent()).toBe(false);
    });
});