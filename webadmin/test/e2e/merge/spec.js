'use strict';

var Page = require('./po.js');
describe('Merge Dialog', function () {
    var p = new Page();

    beforeAll(function(){
        p.get();
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
        p.passwordInput.sendKeys('1');

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
        p.passwordInput.clear();

        expect(p.findSystemButton.isEnabled()).toBe(false);

        p.passwordInput.sendKeys('1');

        expect(p.findSystemButton.isEnabled()).toBe(true);
    });

    it("rejects attempt to merge with non-existing system", function() {
        p.urlInput.sendKeys('http://good.url');
        p.passwordInput.sendKeys(p.password);
        p.findSystemButton.click();
        var alertDialog = element.all(by.css('.modal-dialog')).get(1);
        expect(alertDialog.getText()).toContain('Connection failed: System is unreachable');
        alertDialog.element(by.buttonText('Close')).click();
    });

    it("rejects attempt to merge using wrong other system password", function() {
        p.urlInput.sendKeys(p.activeSystem);
        p.passwordInput.sendKeys('wrong');
        p.findSystemButton.click();
        var alertDialog = element.all(by.css('.modal-dialog')).get(1);
        expect(alertDialog.getText()).toContain('Connection failed: Wrong password.');
        alertDialog.element(by.buttonText('Close')).click();
    });

    it("rejects attempt to merge if other system has incompatible version", function() {
        p.urlInput.sendKeys(p.incompatibleSystem);
        p.passwordInput.sendKeys(p.password);
        p.findSystemButton.click();
        var alertDialog = element.all(by.css('.modal-dialog')).get(1);
        expect(alertDialog.getText()).toContain('Connection failed: Found system has incompatible version.');
        alertDialog.element(by.buttonText('Close')).click();
    });
    
    it("After other system access is gained, it is possible to select another system in dropdown and Merge with it", function() {
        p.urlInput.sendKeys(p.activeSystem);
        p.passwordInput.sendKeys(p.password);
        p.findSystemButton.click();
        p.currentPasswordInput.sendKeys(p.password);

        p.urlInput.clear()
            .sendKeys(p.incompatibleSystem);

        expect(p.mergeSystemsButton.isEnabled()).toBe(true);
        p.mergeSystemsButton.click();
        var alertDialog = element.all(by.css('.modal-dialog')).get(1);
        expect(alertDialog.getText()).toContain('Merge failed: Found system has incompatible version.');
        alertDialog.element(by.buttonText('Close')).click();
    });

    it("rejects attempt to merge using wrong current system password", function() {
        p.urlInput.sendKeys(p.activeSystem);
        p.passwordInput.sendKeys(p.password);
        p.findSystemButton.click();
        p.currentPasswordInput.sendKeys('wrong');
        p.mergeSystemsButton.click();
        var alertDialog = element.all(by.css('.modal-dialog')).get(1);
        expect(alertDialog.getText()).toContain('Merge failed: Incorrect current password');
        alertDialog.element(by.buttonText('Close')).click();
    });

    it("Without current system password merge button is disabled", function() {
        p.urlInput.sendKeys(p.activeSystem);
        p.passwordInput.sendKeys(p.password);
        p.findSystemButton.click();
        p.currentPasswordInput.clear();
        expect(p.mergeSystemsButton.isEnabled()).toBe(false);
    });

    it("Both systems can be selected to use their name and password", function() {
        // Connect to active system
        p.urlInput.sendKeys(p.activeSystem);
        p.passwordInput.sendKeys(p.password);
        p.findSystemButton.click();
        p.currentPasswordInput.sendKeys(p.password);

        // Select another system
        p.extarnalSystemCheckbox.click();
        expect(p.mergeSystemsButton.isEnabled()).toBe(true);
        expect(p.currentSystemCheckbox.isSelected()).toBe(false);
        expect(p.extarnalSystemCheckbox.isSelected()).toBe(true);

        // Select our system back
        p.currentSystemCheckbox.click();
        expect(p.mergeSystemsButton.isEnabled()).toBe(true);
        expect(p.currentSystemCheckbox.isSelected()).toBe(true);
        expect(p.extarnalSystemCheckbox.isSelected()).toBe(false);
    });

    it("displays Merge button, current system password and select of main system after other system access is gained",function(){
        // Merge button and system select are not visible, before any system is discovered
        expect(p.findSystemButton.isDisplayed()).toBe(true);
        expect(p.mergeSystemsButton.isDisplayed()).toBe(false);
        expect(p.currentSystemCheckbox.isDisplayed()).toBe(false);

        // Connect to active system
        p.urlInput.sendKeys(p.activeSystem);
        p.passwordInput.sendKeys(p.password);
        p.findSystemButton.click();

        // New controls are visible now
        expect(p.mergeSystemsButton.isDisplayed()).toBe(true);
        expect(p.currentSystemCheckbox.isDisplayed()).toBe(true);
        expect(p.currentPasswordInput.isEnabled()).toBe(true);
        expect(p.extarnalSystemCheckbox.isDisplayed()).toBe(true);

        p.currentPasswordInput.sendKeys(p.password);
        expect(p.mergeSystemsButton.isEnabled()).toBe(true);
        expect(p.currentSystemCheckbox.isSelected()).toBe(true);
        expect(p.extarnalSystemCheckbox.isSelected()).toBe(false);
    });

    it("Find system button is disabled if url input is cleared",function(){
        p.urlInput.sendKeys(p.activeSystem);
        p.passwordInput.sendKeys(p.password);
        p.findSystemButton.click();
        p.urlInput.clear();
        expect(p.findSystemButton.isEnabled()).toBe(false);
        expect(p.mergeSystemsButton.isEnabled()).toBe(false);
        expect(p.mergeSystemsButton.isDisplayed()).toBe(true);
    });
});