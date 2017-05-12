'use strict';

var SystemsListPage = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    var self = this;

    this.url = '/systems';

    this.activeMenuItem = element(by.css('.navbar-nav')).element(by.css('.active'));
    this.systemsList = element.all(by.repeater('system in systems'));

    //These functions allows to find desired child in provided element
    this.openInNxButton = function(ancestor) {
        return ancestor.element(by.partialButtonText('Open in '));
    };
    this.systemOwner = function(ancestor) {
        return ancestor.element(by.css('.user-name'));
    };

    this.customMatchers = {
        toContainAnyOf: function() {
            return {
                compare: function(actual, expecteds) {
                    if (expecteds === undefined) {
                        expecteds = '';
                    }
                    var result = {};
                    result.message = "Expected " + actual + " to contain any of " + expecteds;
                    for (var i = 0, l = expecteds.length; i < l; i++) {
                        if (self.helper.isSubstr(actual, expecteds[i])) {
                            result.pass = true;
                            result.message = "True: Expected " + actual + " to contain one of " + expecteds;
                            break;
                        }
                    }
                    return result;
                }
            }
        }
    };
};

module.exports = SystemsListPage;