'use strict';

var SystemsListPage = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    this.getHomePage = function () {
        browser.get('/');
        browser.waitForAngular();
    };

    this.get = function () {
        browser.get('/#/systems');
        browser.waitForAngular();
    };

    this.htmlBody = element(by.css('body'));

    this.systemsList = element.all(by.repeater('system in systems'));
};

module.exports = SystemsListPage;