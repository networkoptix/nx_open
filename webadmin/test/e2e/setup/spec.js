'use strict';
var Page = require('./po.js');
describe('Setup Wizard', function () {
    var p = new Page();

    beforeEach(function() {
        p.get();
        //  If setup wizard is present, complete setup
        p.setupDialog.isPresent().then( function(isPresent) {
            if(isPresent) {
                p.helper.completeSetup(); }
        });
    });

    it("Button Restore Factory Defaults opens detach dialog",function(){
        expect(p.mediaServersLinks.count()).toBeGreaterThan(0);

        p.mediaServersLinks.count().then( function(number) {
            if (number == 1) {
                expect(p.resetButton.isPresent()).toBe(true);
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
        expect(p.dialog.getText()).toContain('Wrong password');
        p.closeButton.click();
        p.get();
        expect(p.dialog.isPresent()).toBe(false);
        expect(p.setupDialog.isPresent()).toBe(false); // Fails until WEB-309 is fixed
    });

    it("Setup Wizard Dialog opens after server detach",function(){
        p.triggerSetupWizard();
        p.helper.completeSetup();
    }, 120000);

    it("Start but not complete setup - wizard should appear automatically",function(){
        p.triggerSetupWizard();
        browser.refresh();
        browser.sleep(1000);
        p.helper.waitIfNotDisplayed(p.setupDialog, 1000);
        p.setupDialog.isPresent().then(function(is) {console.log(is, 'setup dialog is present or not')});
        expect(p.setupDialog.isPresent()).toBe(true);
        p.helper.completeSetup();
    });

    it("Every input is required",function(){
        var checkRequired = function(message) {
            p.nextButton.click();
            expect(p.setupDialog.getText()).toContain(message);
        };

        p.triggerSetupWizard();
        browser.sleep(1000);
        p.nextButton.click();

        // System name screen
        p.systemNameInput.clear();
        p.nextButton.click();
        checkRequired("System name is required");
        p.systemNameInput.clear().sendKeys('autotest-system');

        // Merge with existing screen
        p.mergeWithExisting.click();

        p.remoteSystemInput.sendKeys(p.helper.activeSystem);
        p.remotePasswordInput.clear();
        checkRequired("All fields are required");

        p.remoteSystemInput.clear();
        p.remotePasswordInput.sendKeys('password');
        checkRequired("All fields are required");

        p.remotePasswordInput.sendKeys('password');
        p.backButton.click();

        // Connect to cloud screen
        p.nextButton.click();
        p.systemTypeRightButton.click();  // choose cloud system type
        p.nextButton.click();

        // cloud login screen
        p.useCloudAccButton.click();

        p.cloudEmailInput.sendKeys(p.helper.cloudEmail);
        p.cloudPassInput.clear();
        checkRequired("Email and password are required");

        p.cloudEmailInput.clear();
        p.cloudPassInput.sendKeys('password');
        checkRequired("Email and password are required");

        p.backButton.click();
        p.backButton.click();

        // Local login screen
        p.systemTypeRightButton.click(); // choose local system type
        p.nextButton.click();

        p.localPasswordInput.sendKeys(p.helper.password);
        p.localPasswordConfInput.clear();
        checkRequired("All fields are required");

        p.localPasswordInput.clear();
        p.localPasswordConfInput.sendKeys(p.helper.password);
        checkRequired("All fields are required");

        p.localPasswordInput.sendKeys(p.helper.password);
        p.nextButton.click();
        p.finishButton.click();
    });

    it("First field on every step is auto-focused",function(){
        p.triggerSetupWizard();
        browser.sleep(1000);
        p.nextButton.click();

        // System name screen
        p.helper.checkElementFocusedBy(p.systemNameInput, 'model');
        p.systemNameInput.clear().sendKeys('autotest-system');

        // Merge with existing screen
        p.mergeWithExisting.click();
        p.helper.checkElementFocusedBy(p.remoteSystemInput, 'model');
        p.backButton.click();

        // Connect to cloud screen
        p.nextButton.click();
        p.systemTypeRightButton.click();  // choose cloud system type
        p.nextButton.click();

        // cloud login screen
        p.useCloudAccButton.click();
        p.helper.checkElementFocusedBy(p.cloudEmailInput, 'model');
        p.backButton.click();
        p.backButton.click();

        // Local login screen
        p.systemTypeRightButton.click(); // choose local system type
        p.nextButton.click();
        p.helper.checkElementFocusedBy(p.localPasswordInput, 'model');
        p.localPasswordInput.sendKeys(p.helper.password);
        p.localPasswordConfInput.sendKeys(p.helper.password);
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        p.nextButton.click();
        p.finishButton.click();
    });

    it("Back buttons work",function(){
        p.triggerSetupWizard();
        p.nextButton.click();
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Get Started with Nx Witness');
        p.nextButton.click();

        p.advancedSysSett.click();
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Give your System a name');

        p.mergeWithExisting.click();
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Give your System a name');

        p.systemNameInput.clear().sendKeys('autotest-system');
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Get Started with Nx Witness');
        p.nextButton.click();

        // On System name screen
        p.nextButton.click();
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Give your System a name');
        p.nextButton.click();

        // On system type screen - local
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Give your System a name');
        p.nextButton.click();

        // On system type screen - cloud
        p.systemTypeRightButton.click(); // choose cloud system type
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Give your System a name');

        p.nextButton.click();
        p.systemTypeRightButton.click(); // choose cloud system type

        // On Cloud login screen
        p.nextButton.click();
        p.useCloudAccButton.click();
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('To use cloud system you should have an account on Nx Cloud');

        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Choose system type');
        p.systemTypeRightButton.click(); // choose local system type
        p.nextButton.click();

        // On local login screen
        p.backButton.click();
        expect(p.setupDialog.getText()).toContain('Choose system type');
        p.nextButton.click();

        p.localPasswordInput.sendKeys(p.helper.password);
        p.localPasswordConfInput.sendKeys(p.helper.password);
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        p.nextButton.click();
        p.finishButton.click();
    });

    // Does not work locally
    xit("Enter works for next button in every place next is available: advanced settings",function(){
        p.triggerSetupWizard();
        browser.sleep(1000);
        p.pressEnter();

        p.advancedSysSett.click();
        p.pressEnter();
        p.pressEnter();

        p.systemNameInput.clear().sendKeys('autotest-system');
        p.pressEnter();
        p.pressEnter();

        p.localPasswordInput.sendKeys(p.helper.password);
        p.localPasswordConfInput.sendKeys(p.helper.password);
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        p.pressEnter();
        p.pressEnter();
    });

    // Does not work locally
    xit("Enter works for next button in every place next is available: cloud connect",function(){
        p.triggerSetupWizard();
        browser.sleep(1000);
        p.pressEnter();
        p.systemNameInput.clear().sendKeys('autotest-system');
        p.pressEnter();

        p.systemTypeRightButton.click();  // choose cloud system type
        p.pressEnter();
        p.useCloudAccButton.click();
        p.cloudEmailInput.sendKeys(p.helper.cloudEmail);
        p.cloudPassInput.sendKeys(p.helper.password);
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        browser.actions().
            sendKeys(protractor.Key.ENTER).
            perform();

        browser.sleep(1000);
        browser.actions().
            sendKeys(protractor.Key.ENTER).
            perform();

        p.helper.logout();
        p.get();
        p.helper.login('admin', p.helper.password);
    });

    // Does not work locally
    xit("Enter works for next button in every place next is available: merge with another",function(){
        p.triggerSetupWizard();
        browser.sleep(1000);
        browser.actions().
            sendKeys(protractor.Key.ENTER).
            perform();

        p.mergeWithExisting.click();
        p.remoteSystemInput.sendKeys(p.helper.activeSystem);
        p.remotePasswordInput.sendKeys(p.helper.password);
        browser.actions().
            sendKeys(protractor.Key.ENTER).
            perform();

        browser.sleep(1500);
        p.finishButton.click();
    });

    // TODO: Enable when https://networkoptix.atlassian.net/browse/WEB-408 is fixed
    xit("Run setup again after local setup using link (#/setup) - it shows success depending on cloud settings",function(){
        p.triggerSetupWizard();
        p.helper.completeSetup();
        browser.get('/#/setup');
        expect(p.setupDialog.getText()).toContain('System is ready for use');
        p.finishButton.click();
    });

    // TODO: Enable when https://networkoptix.atlassian.net/browse/WEB-408 is fixed
    xit("Run setup again after cloud setup using link (#/setup) - it shows success depending on cloud settings",function(){
        p.triggerSetupWizard();
        p.helper.completeSetupWithCloud();
        browser.get('/#/setup');
        expect(p.setupDialog.getText()).toContain('System is ready for use');
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
        p.systemNameInput.clear().sendKeys('autotest-system');
        p.nextButton.click();
        p.nextButton.click();
        p.localPasswordInput.sendKeys(p.helper.password);
        p.localPasswordConfInput.sendKeys(p.helper.password);
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        p.nextButton.click();
        p.finishButton.click();
    });


    // To run tests with merge, prepare two active systems: compatible and incompatible.
    // Write their ips to file test/e2e/helper.js into variables
    // this.activeSystem and this.incompatibleSystem

    it("Merge: message if wrong login password for a remote system",function(){
        p.triggerSetupWizard();
        expect(p.setupDialog.isDisplayed()).toBe(true);
        browser.sleep(2000);
        p.nextButton.click();
        p.mergeWithExisting.click();
        p.remoteSystemInput.sendKeys(p.helper.activeSystem);
        p.remoteLoginInput.sendKeys('admin');
        p.remotePasswordInput.sendKeys('wrongpassword');
        p.nextButton.click();
        expect(p.setupDialog.getText()).toContain('Wrong password');
        // Go to the beginning of setup wizard
        p.backButton.click();
        p.backButton.click();

        p.helper.completeSetup();
    }, 30000);

    // Does not work locally
    xit("Merge: remote system is not accessible, or wrong url was entered",function(){
        p.triggerSetupWizard();
        expect(p.setupDialog.isDisplayed()).toBe(true);
        browser.sleep(2000);
        p.nextButton.click();
        p.mergeWithExisting.click();
        p.remoteSystemInput.sendKeys('nourl');
        p.remoteLoginInput.sendKeys('admin');
        p.remotePasswordInput.sendKeys(p.helper.password);
        p.nextButton.click();
        expect(p.setupDialog.getText()).toContain('System is unreachable or doesn\'t exist.');
        // Go to the beginning of setup wizard
        p.backButton.click();
        p.backButton.click();

        p.helper.completeSetup();
    });

    // Does not work locally
    xit("Merge: remote system is incompatible", function(){
        p.triggerSetupWizard();
        expect(p.setupDialog.isDisplayed()).toBe(true);
        browser.sleep(2000);
        p.nextButton.click();
        p.mergeWithExisting.click();
        p.remoteSystemInput.sendKeys(p.helper.incompatibleSystem);
        p.remoteLoginInput.sendKeys('admin');
        p.remotePasswordInput.sendKeys(p.helper.password);
        p.nextButton.click();
        expect(p.setupDialog.getText()).toContain('Selected system has incompatible version.');
        // Go to the beginning of setup wizard
        p.backButton.click();
        p.backButton.click();

        p.helper.completeSetup();
    });

    // Does not work locally
    xit("Merge works",function(){
        p.triggerSetupWizard();
        expect(p.setupDialog.isDisplayed()).toBe(true);
        browser.sleep(2000);
        p.nextButton.click();
        p.mergeWithExisting.click();
        p.remoteSystemInput.sendKeys(p.helper.activeSystem);
        p.remoteLoginInput.sendKeys('admin');
        p.remotePasswordInput.sendKeys(p.helper.password);
        p.nextButton.click();
        expect(p.setupDialog.getText()).toContain('System is ready to use');
        p.finishButton.click();
    });

    // TODO repeat for cloud, and for merge
    it("After finishing - you are still logged in into wedamin under new credentials",function(){
        p.triggerSetupWizard();
        browser.sleep(2000);
        p.helper.completeSetup();
        p.logoutButton.click();

        element(by.model('user.username')).sendKeys('admin');
        element(by.model('user.password')).sendKeys(p.helper.password);
        element(by.buttonText('Log in')).click();
        browser.sleep(500);
    });

    it("Cloud connect: Link Learn more visible and works",function(){
        p.triggerSetupWizard();
        expect(p.setupDialog.isDisplayed()).toBe(true);
        p.nextButton.click();
        p.systemNameInput.clear().sendKeys('autotest-system');
        p.nextButton.click();
        p.systemTypeRightButton.click(); // select cloud system type
        p.learnMore.click();
        p.helper.performAtSecondTab( function(){
            expect(browser.getCurrentUrl()).toContain('cloud');
            browser.close();
        });
        p.systemTypeRightButton.click(); // select local system type
        p.nextButton.click();
        p.localPasswordInput.sendKeys(p.helper.password);
        p.localPasswordConfInput.sendKeys(p.helper.password);
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        p.nextButton.click();
        p.finishButton.click();
    }, 90000);

    it("Cloud connect: wrong cloud credentials",function(){
        p.triggerSetupWizard();
        browser.sleep(2000);
        p.nextButton.click();
        p.systemNameInput.clear().sendKeys('autotest-system');
        p.nextButton.click();
        p.systemTypeRightButton.click(); // select cloud system type
        p.nextButton.click();

        p.useCloudAccButton.click();
        p.cloudEmailInput.sendKeys(p.helper.cloudEmail);
        p.cloudPassInput.sendKeys('wrongpass');
        expect(p.setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        p.nextButton.click();
        expect(p.setupDialog.getText()).toContain('Login or password are incorrect');
        p.backButton.click();
        p.backButton.click();
        p.backButton.click();
        p.backButton.click();

        p.helper.completeSetup();
    });

    //it("Handle: server has no internet connection",function(){
    //
    //});
    //it("Handle: client has no internet connection",function(){
    //
    //});
    //it("Success window with admin's email and some usefull hints",function(){
    //
    //});
    //it("After finishing - you are still logged in into wedamin under new credentials",function(){
    //
    //});
    //it("Disconnect from cloud works",function(){
    //
    //});
    //it("third step has a link or button do not connect to cloud",function(){
    //
    //});
    //it("asks for login,password, password confirmation",function(){
    //
    //});
    //it("Password requirements?",function(){
    //
    //});
    //it("Success window with admin's login and system's url",function(){
    //
    //});
    //it("After finishing - you are still logged in into wedamin under new credentials",function(){
    //
    //});
    //it("Error message if repeated password doesn't match",function(){
    //
    //});
});