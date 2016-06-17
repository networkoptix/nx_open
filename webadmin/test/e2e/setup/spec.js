'use strict';
var Page = require('./po.js');
describe('Setup Wizard', function () {
    var p = new Page();

    beforeEach(function() {
        p.get();
        //  If setup wizard is present, complete setup. Otherwise, do nothing
        p.helper.checkPresent(p.setupDialog).then( p.helper.completeSetup, function(){});
    });

    it("If there's only 1 Server in the System, button Reset system opens detach dialog",function(){
        expect(p.mediaServersLinks.count()).toBeGreaterThan(0);
        p.mediaServersLinks.count().then( function(number) {
            if (number == 1) {
                expect(p.resetButton.isDisplayed()).toBe(true);
                p.resetButton.click();
                expect(p.dialog.isDisplayed()).toBe(true);
                p.cancelButton.click();
                expect(p.dialog.isPresent()).toBe(false);
            }
        });
    });

    // This case is unreachable for now. To activate, use system with >1 servers
    xit("If there's more than 1 Server in the System, button Disconnect and create new detaches server",function(){
        expect(p.mediaServersLinks.count()).toBeGreaterThan(0);
        p.mediaServersLinks.count().then( function(number) {
            if (number > 1) {
                expect(p.resetAndNewButton.isDisplayed()).toBe(true);
                p.resetAndNewButton.click();
                expect(p.dialog.isDisplayed()).toBe(true);
                p.cancelButton.click();
                expect(p.dialog.isPresent()).toBe(false);
            }
        });
    });

    it("Server detach button in detach dialog is disabled until password is provided",function(){
        p.resetButton.click();
        p.oldPasswordInput.clear();
        expect(p.detachButton.isEnabled()).toBe(false);
        p.oldPasswordInput.sendKeys(p.helper.password);
        expect(p.detachButton.isEnabled()).toBe(true);
        p.cancelButton.click();
        expect(p.dialog.isPresent()).toBe(false);
    });

    it("Detach dialog Cancel button works",function(){
        p.resetButton.click();
        p.oldPasswordInput.clear();
        p.oldPasswordInput.sendKeys('wrong password');
        p.cancelButton.click();
        expect(p.dialog.isPresent()).toBe(false);
        expect(p.setupDialog.isPresent()).toBe(false);
    });

    it("Detach with wrong password opens dialog with error message",function(){
        p.resetButton.click();
        p.oldPasswordInput.clear();
        p.oldPasswordInput.sendKeys('wrong password');
        p.detachButton.click();
        expect(p.dialog.getText()).toContain('Wrong current password specified');
        p.closeButton.click();
        p.get();
        expect(p.dialog.isPresent()).toBe(false);
        expect(p.setupDialog.isPresent()).toBe(false); // Fails until WEB-309 is fixed
    });

    it("Setup Wizard Dialog opens after server detach",function(){
        p.triggerSetupWizard();
        p.helper.completeSetup();
    });

    // needs to be extended to all branches
    xit("Next button is disabled until every input is filled",function(){
        p.triggerSetupWizard();
        p.nextButton.click();
        p.systemNameInput.clear();
        expect(p.nextButton.isEnabled()).toBe(false);
        p.systemNameInput.sendKeys('autotest-system');
        p.nextButton.click();
        p.skipCloud.click();
        expect(p.nextButton.isEnabled()).toBe(false);
        p.localPasswordInput.sendKeys(p.helper.password);
        expect(p.nextButton.isEnabled()).toBe(false);
        p.localPasswordConfInput.sendKeys(p.helper.password);
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        p.nextButton.click();
        p.finishButton.click();
    });

    xit("First field on every step is auto-focused",function(){
        p.helper.checkElementFocusedBy(p.systemNameInput, 'model');
    });

    it("Link Learn more visible and works",function(){
        p.triggerSetupWizard();
        expect(p.setupDialog.isDisplayed()).toBe(true);
        p.nextButton.click().then(function() {console.log('clicked next');});
        p.systemNameInput.sendKeys('autotest-system');
        p.nextButton.click();
        p.learnMore.click();
        p.helper.performAtSecondTab( function(){
            expect(browser.getCurrentUrl()).toContain('cloud');
            browser.close();
        });
        p.skipCloud.click();
        p.localPasswordInput.sendKeys(p.helper.password);
        p.localPasswordConfInput.sendKeys(p.helper.password);
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        p.nextButton.click();
        p.finishButton.click();
    });
});