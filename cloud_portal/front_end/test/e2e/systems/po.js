'use strict';

var SystemsListPage = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    this.url = '/#/systems';

    this.activeMenuItem = element(by.css('.navbar-nav')).element(by.css('.active'));
    this.systemsList = element.all(by.repeater('system in systems'));

    //These functions allows to find desired child in provided element
    this.openInNxButton = function(ancestor) {
        return ancestor.element(by.buttonText('Open in Nx Witness'));
    };
    this.systemOwner = function(ancestor) {
        return ancestor.element(by.css('.user-name'));
    };
};

module.exports = SystemsListPage;