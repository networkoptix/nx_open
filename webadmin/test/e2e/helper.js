'use strict';

/*
 Tests cheat-sheet:

 Use x to disable suit or case, e.g.
 xdescribe - to disable all specs in the suit
 xit - to disable one spec

 Use f to run only one suit or case, e.g.
 fdescribe  - to run only this suit
 fit - to run only this spec
 */

var Helper = function () {

    var self = this;

    this.activeSystem = 'http://10.0.3.196:7001';
    this.incompatibleSystem = 'http://10.0.3.202:7001';
    this.password = 'qweasd123';
    this.cloudEmail = 'noptixqa+owner@gmail.com';

    this.checkElementFocusedBy = function(element, attribute) {
        expect(element.getAttribute(attribute)).toEqual(browser.driver.switchTo().activeElement().getAttribute(attribute));
    };

    //this.isSubstr = function(string, substring) {
    //    if (string.indexOf(substring) > -1) return true;
    //};

    this.checkPresent = function(elem) {
        var deferred = protractor.promise.defer();
        elem.isPresent().then( function(isPresent) {
            if(isPresent)   deferred.fulfill();
            else            deferred.reject('Element is not present');
        });

        return deferred.promise;
    };

    this.checkDisplayed = function(elem) {
        var deferred = protractor.promise.defer();
        elem.isDisplayed().then( function(isDisplayed) {
            if(isDisplayed)   deferred.fulfill();
            else            deferred.reject('Element is not displayed');
        });

        return deferred.promise;
    };

    this.waitIfNotPresent = function(elem, timeout) {
        var timeoutUsed = timeout || 1000;
        self.checkPresent(elem).then( null, function(err) {
            console.log(err);
            browser.sleep( timeoutUsed );
        });
    };

    this.waitIfNotDisplayed = function(elem, timeout) {
        var timeoutUsed = timeout || 1000;
        self.checkDisplayed(elem).then( null, function(err) {
            console.log(err);
            browser.sleep( timeoutUsed );
        });
    };

    this.closeDialogIfPresent = function(dialog, closeButton) {
        self.checkPresent(dialog).then( function() {
            closeButton.click();
        });
    };

    this.performAtSecondTab = function( callback ) {
        browser.getAllWindowHandles().then(function (handles) {
            var oldWindowHandle = handles[0];
            var newWindowHandle = handles[1];

            // Switch to just opened new tab
            browser.switchTo().window(newWindowHandle);
            if (typeof callback === 'function') {
                callback();
            }
            //Switch back
            browser.switchTo().window(oldWindowHandle);
        });
    };

    this.ignoreSyncFor = function( callback ) {
        browser.ignoreSynchronization = true;
        if (typeof callback === 'function') callback();
        browser.ignoreSynchronization = false;
    };

    this.setupWizardDialog = element(by.css('.modal')).element(by.css('.panel'));
    this.completeSetup = function() {
        var setupDialog = element(by.css('.modal')).element(by.css('.panel'));
        var nextButton = self.setupWizardDialog.element(by.cssContainingText('button', 'Next'));
        var systemNameInput = self.setupWizardDialog.element(by.model('settings.systemName'));
        var skipCloud = self.setupWizardDialog.element(by.linkText('Skip this step for now'));
        var localPasswordInput = self.setupWizardDialog.element(by.model('ngModel'));
        //var localPasswordInput = self.setupWizardDialog.element(by.model('settings.localPassword'));
        var localPasswordConfInput = self.setupWizardDialog.element(by.model('settings.localPasswordConfirmation'));
        var finishButton = self.setupWizardDialog.element(by.buttonText('Finish'));

        expect(setupDialog.isDisplayed()).toBe(true);
        nextButton.click();
        systemNameInput.sendKeys('autotest-system');
        nextButton.click();
        skipCloud.click();
        localPasswordInput.sendKeys(self.password);
        localPasswordConfInput.sendKeys(self.password);
        expect(setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        nextButton.click();
        finishButton.click();
        self.checkPresent(setupDialog).then( function(){ finishButton.click() }, function(){});
    }
};

module.exports = Helper;