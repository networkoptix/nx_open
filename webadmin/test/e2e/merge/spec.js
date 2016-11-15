'use strict';

var Page = require('./po.js');
describe('Merge Dialog', function () {
    var p = new Page();

    beforeAll(function(){
        p.get();
        p.helper.getTab("System").click();
    });

    beforeEach(function(){
        expect(p.mergeButton.isDisplayed()).toBe(true);
        expect(p.mergeButton.isEnabled()).toBe(true);
        p.mergeButton.click();
    });

    afterEach(function(){
        p.dialogCloseButton.click();
    });

    it("can be opened",function(){
        p.helper.waitIfNotPresent(p.mergeDialog, 1000);

        expect(p.mergeDialog.isDisplayed()).toBe(true);
    });

    it("should suggest servers or show message in dropdown",function(){

        p.helper.waitIfNotPresent( (element(by.id("otherSystemUrl"))), 1000);

        var urls = p.systemSuggestionsList.all(by.repeater("system in systems.discoveredUrls"));
        var nomessage = p.systemSuggestionsList.element(by.id("no-systems-message"));

        urls.count().then(function(val){
            if(val == 0) {
                expect(nomessage.isPresent()).toBe(true);
                //expect(nomessage.getText()).toEqual("No systems found");
            }
            else {
                expect(nomessage.isPresent()).toBe(false);
                // Regexp {{system.name}} ({{system.ip}}) ({{system.systemName}}), Server (192.168.0.25) (testFPS250)
                expect(urls.first().element(by.css('a')).getInnerHtml()).toMatch(/[\w\s]+\s+\(\d+\.\d+\.\d+\.\d+\)\s+\(([\w\s\W]+)\)/);
            }
        });
    });

    it("should validate url",function(){
        p.urlInput.clear();
        p.loginInput.sendKeys(p.helper.admin);
        p.passwordInput.sendKeys(p.helper.password);

        expect(p.findSystemButton.isEnabled()).toBe(false);

        p.urlInput.sendKeys("some bad url");

        expect(p.findSystemButton.isEnabled()).toBe(false);

        p.urlInput.clear();
        p.urlInput.sendKeys("http://good.url");

        expect(p.findSystemButton.isEnabled()).toBe(true);

        p.urlInput.clear();
        p.urlInput.sendKeys("http://127.0.0.1:8000");

        expect(p.findSystemButton.isEnabled()).toBe(true);

        p.urlInput.clear();
        p.urlInput.sendKeys("http://localhost");

        expect(p.findSystemButton.isEnabled()).toBe(true);
    });

    it("should require password",function(){
        p.urlInput.sendKeys("http://good.url");
        p.loginInput.sendKeys(p.helper.admin);
        p.passwordInput.clear();

        expect(p.findSystemButton.isEnabled()).toBe(false);

        p.passwordInput.sendKeys(p.helper.password);

        expect(p.findSystemButton.isEnabled()).toBe(true);
    });

    // Fails, because request  /proxy/https/good.url/web/api/getNonce?userName=admin fails silently
    xit("rejects attempt to merge with non-existing system", function() {
        p.urlInput.sendKeys('http://good.url');
        p.loginInput.sendKeys(p.helper.admin);
        p.passwordInput.sendKeys(p.helper.password);
        p.findSystemButton.click();
        p.helper.checkContainsString(p.dialog, 'Connection failed: System is unreachable').then( function() {
            p.dialog.element(by.buttonText('Close')).click();
        });
    });

    it("rejects attempt to merge using wrong other system password", function() {
        p.urlInput.sendKeys(p.helper.activeSystem);
        p.loginInput.sendKeys(p.helper.admin);
        p.passwordInput.sendKeys('wrong');
        p.findSystemButton.click();
        expect(p.alertDialog.getText()).toContain('Connection failed: Login or password are incorrect');
        p.alertDialog.element(by.buttonText('Close')).click();
    });

    it("rejects attempt to merge if other system has incompatible version", function() {
        p.urlInput.sendKeys(p.helper.incompatibleSystem);
        p.loginInput.sendKeys(p.helper.admin);
        p.passwordInput.sendKeys(p.helper.password);
        p.findSystemButton.click();
        p.helper.checkPresent(p.alertDialog).then(function() {
            expect(p.alertDialog.getText()).toContain('Connection failed: Found system has incompatible version.');
            p.alertDialog.element(by.buttonText('Close')).click();
        }, function (err) {
            console.log(err);
            fail('No alert appeared, this must be a bug');
        });
    });
    
    it("After other system access is gained, it is possible to select another system in dropdown and Merge with it", function() {
        p.urlInput.sendKeys(p.helper.activeSystem);
        p.loginInput.sendKeys(p.helper.admin);
        p.passwordInput.sendKeys(p.helper.password);
        p.findSystemButton.click();
        p.currentPasswordInput.sendKeys(p.helper.password);

        p.urlInput.clear()
            .sendKeys(p.helper.incompatibleSystem);

        expect(p.mergeSystemsButton.isEnabled()).toBe(true);
        p.mergeSystemsButton.click();
        p.helper.checkPresent(p.alertDialog).then(function() {
            expect(p.alertDialog.getText()).toContain('Merge failed: Found system has incompatible version.');
            p.alertDialog.element(by.buttonText('Close')).click();
        }, function (err) {
            console.log(err);
            fail('No alert appeared, this must be a bug');
        });
    });

    it("rejects attempt to merge using wrong current system password", function() {
        p.urlInput.sendKeys(p.helper.activeSystem);
        p.loginInput.sendKeys(p.helper.admin);
        p.passwordInput.sendKeys(p.helper.password);
        p.findSystemButton.click();
        p.currentPasswordInput.sendKeys('wrong');
        p.mergeSystemsButton.click();
        p.helper.checkPresent(p.alertDialog).then(function() {
            expect(p.alertDialog.getText()).toContain('Wrong password');
            p.alertDialog.element(by.buttonText('Close')).click();
        }, function (err) {
            console.log(err);
            fail('No alert appeared, this must be a bug');
        });
    });

    it("Without current system password merge button is disabled", function() {
        p.urlInput.sendKeys(p.helper.activeSystem);
        p.loginInput.sendKeys(p.helper.admin);
        p.passwordInput.sendKeys(p.helper.password);
        p.findSystemButton.click();
        p.currentPasswordInput.clear();
        expect(p.mergeSystemsButton.isEnabled()).toBe(false);
    });

    it("Both systems can be selected to use their name and password", function() {
        // Connect to active system
        p.urlInput.sendKeys(p.helper.activeSystem);
        p.loginInput.sendKeys(p.helper.admin);
        p.passwordInput.sendKeys(p.helper.password);
        p.findSystemButton.click();
        p.currentPasswordInput.sendKeys(p.helper.password);

        // Select another system
        p.externalSystemCheckbox.click();
        expect(p.mergeSystemsButton.isEnabled()).toBe(true);
        expect(p.currentSystemCheckbox.isSelected()).toBe(false);
        expect(p.externalSystemCheckbox.isSelected()).toBe(true);

        // Select our system back
        p.currentSystemCheckbox.click();
        expect(p.mergeSystemsButton.isEnabled()).toBe(true);
        expect(p.currentSystemCheckbox.isSelected()).toBe(true);
        expect(p.externalSystemCheckbox.isSelected()).toBe(false);
    });

    it("displays Merge button, current system password and select of main system after other system access is gained",function(){
        // Merge button and system select are not visible, before any system is discovered
        expect(p.findSystemButton.isDisplayed()).toBe(true);
        expect(p.mergeSystemsButton.isDisplayed()).toBe(false);
        expect(p.currentSystemCheckbox.isDisplayed()).toBe(false);

        // Connect to active system
        p.urlInput.sendKeys(p.helper.activeSystem);
        p.loginInput.sendKeys(p.helper.admin);
        p.passwordInput.sendKeys(p.helper.password);
        p.findSystemButton.click().then(null, function () {
            fail('Button Find System is not clickable.');
        });

        // New controls are visible now
        expect(p.mergeSystemsButton.isDisplayed()).toBe(true);
        expect(p.currentSystemCheckbox.isDisplayed()).toBe(true);
        expect(p.currentPasswordInput.isEnabled()).toBe(true);
        expect(p.externalSystemCheckbox.isDisplayed()).toBe(true);

        p.currentPasswordInput.sendKeys(p.helper.password);
        expect(p.mergeSystemsButton.isEnabled()).toBe(true);
        expect(p.currentSystemCheckbox.isSelected()).toBe(true);
        expect(p.externalSystemCheckbox.isSelected()).toBe(false);
    });

    it("Find system button is disabled if url input is cleared",function(){
        p.urlInput.sendKeys(p.helper.activeSystem);
        p.loginInput.sendKeys(p.helper.admin);
        p.passwordInput.sendKeys(p.helper.password);
        p.findSystemButton.click();
        p.urlInput.clear();
        expect(p.findSystemButton.isEnabled()).toBe(false);
        expect(p.mergeSystemsButton.isEnabled()).toBe(false);
        expect(p.mergeSystemsButton.isDisplayed()).toBe(true);
    });
});