'use strict';

var SettingsPage = require('./po.js');
xdescribe('Cloud connect', function () {

    var p = new SettingsPage();

    beforeAll(function(){
        p.get();
        // if button Connect to Cloud is not present (system is in Cloud),
        // then disconnect from Cloud
        p.helper.checkPresent(p.connectToCloudButton).then( null, function(err) {
            console.log(err);
            p.disconnectFromCloud();
        });
    });

    it("has button Connect to Cloud, when system is not in cloud",function(){
        expect(p.connectToCloudButton.isDisplayed()).toBe(true);
    });

    it("has button Create Account, when system is not in cloud",function(){
        expect(p.createCloudAccButton.isDisplayed()).toBe(true);
    });

    it("button Create account is an external link",function(){
        expect(p.createCloudAccButton.getAttribute('href')).toContain('http://');
        expect(p.createCloudAccButton.getAttribute('href')).toContain('#/register?from=webadmin&context=settings');
    });

    it("button Connect to Cloud opens dialog",function(){
        p.connectToCloudButton.click();
        expect(p.dialog.isDisplayed()).toBe(true);
        expect(p.dialog.$('h3').getText()).toContain('Connect system to Nx Cloud');

        p.cloudDialogCloseButton.click();
    });

    it("Connect dialog: button Connect system is disabled until email input has correct format",function(){
        p.connectToCloudButton.click();
        p.cloudPasswordInput.sendKeys(p.password);
        p.cloudEmailInput.clear()
            .sendKeys('noptixqa+owner');
        expect(p.dialogConnectButton.isEnabled()).toBe(false);

        p.cloudEmailInput.clear()
            .sendKeys('noptixqa+owner@gmail.com');
        expect(p.dialogConnectButton.isEnabled()).toBe(true);

        p.cloudDialogCloseButton.click();
    });

    it("Connect dialog: button Connect system is disabled while password field is empty",function(){
        p.connectToCloudButton.click();
        p.cloudEmailInput.clear()
            .sendKeys('noptixqa+owner@gmail.com');
        p.cloudPasswordInput.clear();
        expect(p.dialogConnectButton.isEnabled()).toBe(false);

        p.cloudPasswordInput.sendKeys(p.password);
        expect(p.dialogConnectButton.isEnabled()).toBe(true);

        p.cloudDialogCloseButton.click();
    });

    it("Connect dialog: empty fields are highlighted with red",function(){
        p.connectToCloudButton.click();
        p.cloudEmailInput.clear();
        p.cloudPasswordInput.clear()
            .sendKeys(protractor.Key.TAB); // blur
        expect(p.cloudEmailInput.element(by.xpath('..')).getAttribute('class')).toContain('has-error');
        expect(p.cloudPasswordInput.element(by.xpath('..')).getAttribute('class')).toContain('has-error');

        p.cloudDialogCloseButton.click();
    });

    it("Connect dialog: wrong format fields are highlighted with red",function(){
        p.connectToCloudButton.click();
        p.cloudEmailInput.clear()
            .sendKeys("notanemail")
            .sendKeys(protractor.Key.TAB);
        expect(p.cloudEmailInput.element(by.xpath('..')).getAttribute('class')).toContain('has-error');

        p.cloudDialogCloseButton.click();
    });

    it("Connect dialog: incorrect email+password pair triggers error message",function(){
        p.connectToCloudButton.click();
        p.cloudEmailInput.sendKeys('noptixqa+owner@gmail.com');
        p.cloudPasswordInput.sendKeys('wrongpassword');
        p.dialogConnectButton.click();
        expect(p.dialogMessage.getText()).toContain('Login or password are incorrect');

        p.cloudDialogCloseButton.click();
    });

    it("Connect dialog: can be closed with close button",function(){
        p.connectToCloudButton.click();
        p.cloudDialogCloseButton.click();
    });

    it("dialog: can be closed with cancel button",function(){
        p.connectToCloudButton.click();
        p.dialogCancelButton.click();
    });

    it("dialog: after connect success message appears", function() {
        p.connectToCloudButton.click();
        p.cloudEmailInput.sendKeys('noptixqa+owner@gmail.com');
        p.cloudPasswordInput.sendKeys(p.password);
        p.dialogConnectButton.click();
        expect(p.dialogMessage.getText()).toContain('System is connected to ');
        p.cloudDialogCloseButton.click();
        p.disconnectFromCloud();
    });

    it("dialog: after connect click Connect again - error appears", function(){
        p.connectToCloudButton.click();
        p.cloudEmailInput.sendKeys('noptixqa+owner@gmail.com');
        p.cloudPasswordInput.sendKeys(p.password);
        p.dialogConnectButton.click();
        expect(p.dialogMessage.getText()).toContain('System is connected to ');
        // Click Connect button again
        p.dialogConnectButton.click();
        expect(p.dialogMessage.getText()).toContain('Can\'t connect: some error happened: System already bound to cloud ');
        p.cloudDialogCloseButton.click();
        p.disconnectFromCloud();
    });
});