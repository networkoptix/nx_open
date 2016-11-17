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

    this.activeSystem = 'http://10.0.2.15:7001';
    this.incompatibleSystem = 'http://10.0.3.52:7001';
    this.admin = 'admin';
    this.password = 'qweasd123';
    this.cloudEmail = 'noptixqa+owner@gmail.com';

    this.elementAbsentMessage = 'Element is not present, but this was expected!';
    this.stringAbsentMessage = 'Element does not contain such string, but this was expected!';

    this.checkElementFocusedBy = function(element, attribute) {
        expect(element.getAttribute(attribute)).toEqual(browser.driver.switchTo().activeElement().getAttribute(attribute));
    };

    this.isSubstr = function(string, substring) {
        if (string.indexOf(substring) > -1) return true;
    };

    this.checkPresent = function(elem) {
        var deferred = protractor.promise.defer();
        elem.isPresent().then( function(isPresent) {
            if(isPresent)   deferred.fulfill();
            else            deferred.reject(self.elementAbsentMessage);
        });

        return deferred.promise;
    };

    this.checkDisplayed = function(elem) {
        var deferred = protractor.promise.defer();
        elem.isDisplayed().then( function(isDisplayed) {
            if(isDisplayed)   deferred.fulfill();
            else            deferred.reject(self.elementAbsentMessage);
        });

        return deferred.promise;
    };

    this.checkContainsString = function(elem, string) {
        var deferred = protractor.promise.defer();
        elem.getText().then( function(text) {
            if(self.isSubstr(text, string))   deferred.fulfill();
            else            deferred.reject(self.stringAbsentMessage);
        });

        return deferred.promise;
    };

    this.waitIfNotPresent = function(elem, timeout) {
        var timeoutUsed = timeout || 1000;
        self.checkPresent(elem).then( null, function(err) {
            console.log(err, 'Waiting until it will be present');
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
        var localPasswordInput = self.setupWizardDialog.element(by.model('ngModel'));
        //var localPasswordInput = self.setupWizardDialog.element(by.model('settings.localPassword'));
        var localPasswordConfInput = self.setupWizardDialog.element(by.model('settings.localPasswordConfirmation'));
        var finishButton = self.setupWizardDialog.element(by.buttonText('Finish'));

        expect(setupDialog.isDisplayed()).toBe(true);
        nextButton.click();
        systemNameInput.clear().sendKeys('autotest-system');
        nextButton.click();
        nextButton.click();
        localPasswordInput.sendKeys(self.password);
        localPasswordConfInput.sendKeys(self.password);
        expect(setupDialog.isDisplayed()).toBe(true); // without this, exception is thrown. magic.
        nextButton.click();
        finishButton.click();
        self.checkPresent(setupDialog).then( function(){ finishButton.click() }, function(){});
    };

    this.completeSetupWithCloud = function() {
        var setupDialog = element(by.css('.modal')).element(by.css('.panel'));
        var nextButton = self.setupWizardDialog.element(by.cssContainingText('button', 'Next'));
        var systemNameInput = self.setupWizardDialog.element(by.model('settings.systemName'));
        var finishButton = self.setupWizardDialog.element(by.buttonText('Finish'));
        var useCloudAccButton = self.setupWizardDialog.element(by.buttonText('Use existing'));
        var cloudEmailInput = self.setupWizardDialog.element(by.model('settings.cloudEmail'));
        var cloudPassInput = self.setupWizardDialog.element(by.model('settings.cloudPassword'));
        var systemTypeRightButton = self.setupWizardDialog.element(by.css('.pull-right'));

        expect(setupDialog.isDisplayed()).toBe(true);
        nextButton.click();
        systemNameInput.clear().sendKeys('autotest-system');
        nextButton.click();
        systemTypeRightButton.click();  // choose cloud system type
        nextButton.click();
        useCloudAccButton.click();
        cloudEmailInput.sendKeys(self.cloudEmail);
        cloudPassInput.sendKeys(self.password);
        nextButton.click();
        finishButton.click();
    };

    this.logout = function() {
        element(by.css('[title=Logout]')).click();
    };

    this.login = function(login, password) {
        var username = login || 'admin';
        var passwd = password || self.password;
        element(by.model('user.username')).isPresent().then( function(isPresent) {
            if(isPresent) {
                // if there's login dialog
                element(by.model('user.username')).clear().sendKeys(username);
                element(by.model('user.password')).clear().sendKeys(passwd);
                element(by.buttonText('Log in')).click();

                browser.sleep(500);
            }
        });
    };

    // accepts array with objects, like [['login1', 'password1'], ['login2', 'password2']]
    this.attemptLogin = function (credentialsArray) {
        var closeButton = element(by.buttonText('Close'));
        var incorrectPassMess = "Login or password is incorrect";

        var args = Array.prototype.slice.call(arguments);

        (function process(i) {
            // finish loop when range is finished
            if (i >= args.length) return;

            // reject if credentials are not strings or not full
            if ((typeof(args[i][0]) != "string" || typeof(args[i][1]) != "string") &&
                (typeof(args[i][0]) == "undefined" || typeof(args[i][1]) == "undefined")) {
                console.log('Skipping this credentials pair, because login or password are not strings or missing: '
                    + args[i][0] + ' (login) is "' + typeof(args[i][0]) + '"; and ' + args[i][1] + ' (password) is "' + typeof(args[i][1]) + '".' );
                process(i + 1);
            }

            self.login(args[i][0], args[i][1]);
            self.checkContainsString(element(by.css('body')), incorrectPassMess) // if page contains message about wrong credentials
                .then( function () {
                    closeButton.click();                      // close error message
                    process(i + 1);
                }, function () {}); // if string is not contained, skip silently
        })(0);

    };

    this.getTab = function(tab) {
        return element(by.css('li[heading = "' + tab + '"]'));
    };
};

module.exports = Helper;