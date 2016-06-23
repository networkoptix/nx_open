'use strict';
var Page = require('./po.js');
describe('Setup Wizard', function () {
    var p = new Page();

    beforeEach(function() {
        p.get();
        //  If setup wizard is present, complete setup. Otherwise, do nothing
        p.helper.checkPresent(p.setupDialog).then( p.helper.completeSetup, function(){});
    });

    it("Button Restore Factory Defaults opens detach dialog",function(){
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
        browser.sleep(2000);
        p.helper.completeSetup();
    });

    it("Start but not complete setup - wizard should appear automatically",function(){
        p.triggerSetupWizard();
        browser.refresh();
        browser.sleep(2000);
        p.helper.checkPresent(element(by.model('user.username'))).then( function() {
            browser.refresh();
            browser.sleep(1000);
        });
        this.helper.waitIfNotDisplayed(this.setupDialog, 1000);
        expect(p.setupDialog.isPresent()).toBe(true);
        p.helper.completeSetup();
    });

    // needs to be extended to all branches of setup
    it("Next button is disabled until every input is filled",function(){
        p.triggerSetupWizard();
        browser.sleep(2000);
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

    // needs to be extended to all branches of setup
    it("First field on every step is auto-focused",function(){
        p.triggerSetupWizard();
        p.nextButton.click();
        p.helper.checkElementFocusedBy(p.systemNameInput, 'model');
        p.systemNameInput.sendKeys('autotest-system');
        p.nextButton.click();
        p.skipCloud.click();
        p.helper.checkElementFocusedBy(p.localPasswordInput, 'model');
        p.localPasswordInput.sendKeys(p.helper.password);
        p.localPasswordConfInput.sendKeys(p.helper.password);
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        p.nextButton.click();
        p.finishButton.click();
    });

    // needs to be extended to all branches of setup
    it("Back buttons work",function(){
        p.triggerSetupWizard();
        p.nextButton.click();
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Get Started with Nx Witness');
        p.nextButton.click();
        p.systemNameInput.sendKeys('autotest-system');
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Get Started with Nx Witness');
        p.nextButton.click();

        // On System name screen
        p.nextButton.click();
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Give your System a name');
        p.nextButton.click();

        // On Cloud screen
        p.skipCloud.click();
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Connect your System to Nx Cloud');
        p.skipCloud.click();

        p.localPasswordInput.sendKeys(p.helper.password);
        p.localPasswordConfInput.sendKeys(p.helper.password);
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        p.nextButton.click();
        p.finishButton.click();
    });

    // needs to be extended to all branches of setup
    it("Enter works for next button in every place next is available",function(){
        p.triggerSetupWizard();
        browser.sleep(2000);
        browser.actions().
            sendKeys(protractor.Key.ENTER).
            perform();
        p.systemNameInput.sendKeys('autotest-system');
        browser.actions().
            sendKeys(protractor.Key.ENTER).
            perform();
        p.skipCloud.click();
        p.localPasswordInput.sendKeys(p.helper.password);
        p.localPasswordConfInput.sendKeys(p.helper.password);
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        browser.actions().
            sendKeys(protractor.Key.ENTER).
            perform();
        p.finishButton.click();
    });

    it("Link Learn more visible and works",function(){
        p.triggerSetupWizard();
        expect(p.setupDialog.isDisplayed()).toBe(true);
        p.nextButton.click();
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

    it("Run setup again using link (#/setup) - it shows success depending on cloud settings",function(){
        p.triggerSetupWizard();
        p.helper.completeSetup();
        browser.get('/#/setup');
        expect(p.setupDialog.getText()).toContain('System is ready to use');
        p.finishButton.click();
    });


    it("Advanced settings: checkboxes are on by default; changes are saved",function(){
        p.triggerSetupWizard();
        expect(p.setupDialog.isDisplayed()).toBe(true);
        p.nextButton.click();
        p.advancedSysSett.click();
        expect(p.setupDialog.getText()).toContain('Advanced system settings');

        // Check that all checkboxes are on by default
        expect(p.camOptimCheckbox.isSelected()).toBe(true);
        expect(p.autoDiscovCheckbox.isSelected()).toBe(true);
        expect(p.statisticsCheckbox.isSelected()).toBe(true);

        // Checkboxes are changeable
        p.toggleCheckbox(p.camOptimCheckbox);
        p.toggleCheckbox(p.autoDiscovCheckbox);
        p.toggleCheckbox(p.statisticsCheckbox);
        p.nextButton.click();
        p.advancedSysSett.click(); // go to Advanced settings again
        // Check that changes are saved after step forward (states are inverted)
        expect(p.camOptimCheckbox.isSelected()).toBe(false);
        expect(p.autoDiscovCheckbox.isSelected()).toBe(false);
        expect(p.statisticsCheckbox.isSelected()).toBe(false);

        p.backButton.click();
        p.advancedSysSett.click(); // go to Advanced settings again
        // Check that changes are saved after step back (states are inverted)
        expect(p.camOptimCheckbox.isSelected()).toBe(false);
        expect(p.autoDiscovCheckbox.isSelected()).toBe(false);
        expect(p.statisticsCheckbox.isSelected()).toBe(false);

        p.nextButton.click();
        // Do the rest of the setup
        p.systemNameInput.sendKeys('autotest-system');
        p.nextButton.click();
        p.skipCloud.click();
        p.localPasswordInput.sendKeys(p.helper.password);
        p.localPasswordConfInput.sendKeys(p.helper.password);
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        p.nextButton.click();
        p.finishButton.click();
    });




    it("Merge: message if wrong login password for a remote system",function(){
        p.triggerSetupWizard();
        expect(p.setupDialog.isDisplayed()).toBe(true);
        browser.sleep(2000);
        p.nextButton.click();
        p.mergeWithExisting.click();
        p.remoteSystemInput.sendKeys(p.helper.activeSystem);
        p.remotePasswordInput.sendKeys('wrongpassword');
        p.nextButton.click();
        expect(p.setupDialog.getText()).toContain('Wrong password');
        // Go to the beginning of setup wizard
        p.backButton.click();
        p.backButton.click();

        p.helper.completeSetup();
    });

    fit("Merge: remote system is not accessible",function(){
        p.triggerSetupWizard();
        expect(p.setupDialog.isDisplayed()).toBe(true);
        browser.sleep(2000);
        p.nextButton.click();
        p.mergeWithExisting.click();
        p.remoteSystemInput.sendKeys('nourl');
        p.remotePasswordInput.sendKeys(p.helper.password);
        p.nextButton.click();
        expect(p.setupDialog.getText()).toContain('System is unreachable or doesn\'t exist.');
        // Go to the beginning of setup wizard
        p.backButton.click();
        p.backButton.click();

        p.helper.completeSetup();
    });

    it("Merge works",function(){
        p.triggerSetupWizard();
        expect(p.setupDialog.isDisplayed()).toBe(true);
        browser.sleep(2000);
        p.nextButton.click();
        p.mergeWithExisting.click();
        p.remoteSystemInput.sendKeys(p.helper.activeSystem);
        p.remotePasswordInput.sendKeys(p.helper.password);
        p.nextButton.click();
        expect(p.setupDialog.getText()).toContain('System is ready to use');
        p.finishButton.click();
    });

});