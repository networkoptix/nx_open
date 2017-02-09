'use strict';
var Helper = require('../helper.js');
this.helper = new Helper();
var helper = this.helper;
describe('Smoke suit:', function () {

    //var p = new SomePage();

    beforeAll(function() {
        helper.get();
    });

    it("can try to register existing user noptixqa+owner@gmail.com", function () {
        helper.createUser(null, null, helper.userEmailOwner);
    });

    it("can create new user", function () {
        helper.createUser(null, null, helper.getRandomEmail());
    });

    it("can login and logout", function () {
        helper.login();
        helper.logout();
    });

    it("can share system with existing user", function () {
        var newUserEmail = helper.getRandomEmail();
        helper.createUser('Mark', 'Hamill', newUserEmail);

        helper.login(helper.userEmailOwner);
        helper.shareSystemWith(newUserEmail, helper.roles.admin);
        helper.logout();
    });

    fit("can share system with non-existing user", function () {
        var newUserEmail = helper.getRandomEmail();

        helper.login(helper.userEmailOwner);
        helper.shareSystemWith(newUserEmail, helper.roles.admin);
        helper.logout();
    });
});