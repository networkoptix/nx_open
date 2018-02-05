'use strict';

var Customization = function () {
    var p = this;

    var PasswordFieldSuit = require('../password_check.js');
    this.passwordField = new PasswordFieldSuit();

    var Helper = require('../helper.js');
    this.helper = new Helper();

    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();

    var brandObj = require('../../../test-customizations.json');
    this.brandText = brandObj.text;

    this.languageDropdown = element(by.id('language-dropdown'));

    this.languageOption = element(by.css('li[language-select]')).all(by.repeater('lang in languages'));
    this.russianOption = element(by.css('li[language-select]')).element(by.css('span[lang=ru]'));

    this.welcomeElem = element(by.css(".welcome-caption"));

    this.selectNextLang = function (index) {
        p.languageDropdown.click();
        p.languageOption.get(index).click();
        browser.sleep(2000);
    }
};

module.exports = Customization;