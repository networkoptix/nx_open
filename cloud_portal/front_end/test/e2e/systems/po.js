'use strict';

var SystemsListPage = function () {
    var LoginLogout = require('../login_logout.js');
    this.loginLogout = new LoginLogout();

    this.getHomePage = function () {
        browser.get('/');
        browser.waitForAngular();
    };

    this.get = function () {
        browser.get('/#/systems');
        browser.waitForAngular();
    };

    this.htmlBody = element(by.css('body'));

    this.userEmail = 'ekorneeva+1@networkoptix.com';
    this.userPassword = 'qweasd123';

    this.systemsList = element.all(by.repeater('system in systems'));
};

module.exports = SystemsListPage;