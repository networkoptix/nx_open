'use strict';

var Customization = function () {

    var PasswordFieldSuit = require('../password_check.js');
    this.passwordField = new PasswordFieldSuit();

    var Helper = require('../helper.js');
    this.helper = new Helper();

    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();

    var brandObj = require('../../../test-customization.json');
    this.brandText = brandObj.text;

    this.languageDropdown = element(by.id('language-dropdown'));
    this.russianOption = element(by.css('li[language-select]')).element(by.cssContainingText('a', 'Russia'));

};

module.exports = Customization;