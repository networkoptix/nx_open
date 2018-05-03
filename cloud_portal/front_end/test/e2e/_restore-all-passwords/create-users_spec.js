'use strict';
describe('Create user', function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();
    var self = this;
    
    beforeEach(function() {
        self.helper.get(self.helper.urls.register);
    });

    it("owner", function () {
        var userEmail = self.helper.userEmailOwner;
        self.helper.createUser(null, null, userEmail);
    });

    it("admin", function () {
        var userEmail = self.helper.userEmailAdmin;
        self.helper.createUser(null, null, userEmail);
    });
    it("viewer", function () {
        var userEmail = self.helper.userEmailViewer;

        self.helper.createUser(null, null, userEmail);
    });
    it("advanced viewer", function () {
        var userEmail = self.helper.userEmailAdvViewer;

        self.helper.createUser(null, null, userEmail);
    });
    it("live viewer", function () {
        var userEmail = self.helper.userEmailLiveViewer;

        self.helper.createUser(null, null, userEmail);
    });
    it("custom", function () {
        var userEmail = self.helper.userEmailCustom;

        self.helper.createUser(null, null, userEmail);
    });    
    it("no permissions", function () {
        var userEmail = self.helper.userEmailNoPerm;

        self.helper.createUser(null, null, userEmail);
    });
    it("+1", function () {
        var userEmail = self.helper.userEmail;

        self.helper.createUser(null, null, userEmail);
    });
    it("+2", function () {
        var userEmail = self.helper.userEmail2;

        self.helper.createUser(null, null, userEmail);
    });
});
