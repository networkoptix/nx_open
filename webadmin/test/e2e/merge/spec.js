'use strict';

var Page = require('./po.js');
describe('Merge Dialog', function () {
    it("should stop test",function(){expect("other test").toBe("uncommented");});return;

    var p = new Page();

    it("should open merge dialog",function(){
        p.get();
        expect(p.mergeButton.isDisplayed()).toBe(true);
        expect(p.mergeButton.isEnabled()).toBe(true);
        p.mergeButton.click();
        expect(p.mergeDialog.isDisplayed()).toBe(true);

    });

    it("should suggest servers or show message in dropdown",function(){
        var urls = p.systemSuggestionsList.all(by.repeater("system in systems.discoveredUrls"));

        var nomessage = p.systemSuggestionsList.element(by.id("no-systems-message"));

        urls.count().then(function(val){
            if(val == 0){
                expect(nomessage.isPresent()).toBe(true);
                //expect(nomessage.getText()).toEqual("No systems found");
            }else{
                expect(nomessage.isPresent()).toBe(false);
                expect(url.first().getText()).toMatch(/[\w\d\s]+\s+\(\d+\.\d+\.\d+\.\d+\)\s+([\w\d\s]+)/);//{{system.name}} ({{system.ip}}) ({{system.systemName}})
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

    it("should find system first, after that - allow to join system",function(){
        expect(p.findSystemButton.isDisplayed()).toBe(true);
        expect(p.mergeSystemsButton.isDisplayed()).toBe(false);
        expect(p.currentSystemCheckbox.isDisplayed()).toBe(false);

        p.urlInput.clear();
        p.urlInput.sendKeys("http://192.168.56.101:9000/");
        p.passwordInput.clear();
        p.passwordInput.sendKeys("123");
        p.findSystemButton.click();

        //1. All apeared
        expect(p.mergeSystemsButton.isDisplayed()).toBe(true);
        expect(p.currentSystemCheckbox.isDisplayed()).toBe(true);
        expect(p.extarnalSystemCheckbox.isDisplayed()).toBe(true);

        expect(p.mergeSystemsButton.isEnabled()).toBe(true);
        expect(p.currentSystemCheckbox.isSelected()).toBe(true);
        expect(p.extarnalSystemCheckbox.isSelected()).toBe(false);

        //3. select another system
        p.extarnalSystemCheckbox.click();
        expect(p.mergeSystemsButton.isEnabled()).toBe(true);
        expect(p.currentSystemCheckbox.isSelected()).toBe(false);
        expect(p.extarnalSystemCheckbox.isSelected()).toBe(true);

        //2. select our system back
        p.currentSystemCheckbox.click();
        expect(p.mergeSystemsButton.isEnabled()).toBe(true);
        expect(p.currentSystemCheckbox.isSelected()).toBe(true);
        expect(p.extarnalSystemCheckbox.isSelected()).toBe(false);

    });

    it("should force to find system again if smth changes",function(){
        expect("finding again").toBe("discussed");return;

        p.urlInput.clear();
        expect(p.findSystemButton.isEnabled()).toBe(false);
        expect(p.mergeSystemsButton.isEnabled()).toBe(false);
        expect(p.mergeSystemsButton.isDisplayed()).toBe(true);

        p.urlInput.clear();
        p.urlInput.sendKeys("http://192.168.56.101:9000/");
        expect(p.findSystemButton.isEnabled()).toBe(true);
        expect(p.mergeSystemsButton.isEnabled()).toBe(false);
        expect(p.mergeSystemsButton.isDisplayed()).toBe(true);

        p.findSystemButton.click();
        //We found another system - flush radiobuttons
        expect(p.findSystemButton.isEnabled()).toBe(true);
        expect(p.mergeSystemsButton.isEnabled()).toBe(true);
        expect(p.mergeSystemsButton.isDisplayed()).toBe(true);
    });

    it("should join system",function(){
        expect("merging systems").toBe("tested manually");
    });


});