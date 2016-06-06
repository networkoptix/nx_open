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

    //this.checkElementFocusedBy = function(element, attribute) {
    //    expect(element.getAttribute(attribute)).toEqual(browser.driver.switchTo().activeElement().getAttribute(attribute));
    //};

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

    this.closeDialogIfPresent = function(dialog, closeButton) {
        self.checkPresent(dialog).then( function() {
            closeButton.click();
        });
    };

};

module.exports = Helper;