'use strict';

var SystemsListPage = function () {

    this.getHomePage = function () {
        browser.get('/');
        browser.waitForAngular();
    };

    this.get = function () {
        browser.get('/systems');
        browser.waitForAngular();
    };
};

module.exports = SystemsListPage;