'use strict';

var Customization = function () {

    var PasswordFieldSuit = require('../password_check.js');
    this.passwordField = new PasswordFieldSuit();

    var Helper = require('../helper.js');
    this.helper = new Helper();

    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();
};

module.exports = Customization;