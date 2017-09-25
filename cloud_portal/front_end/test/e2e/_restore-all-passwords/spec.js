'use strict';
describe('Restore all passwords', function () { // to enable suit, replace xdescribe with describe
    var Helper = require('../helper.js');
    this.helper = new Helper();
    var self = this;

    it("restore system owner password", function () {
        self.helper.restorePassword(self.helper.userEmailOwner);
    });
    it("restore system admin password", function () {
        browser.sleep(5000);
        self.helper.restorePassword(self.helper.userEmailAdmin);
    });
    it("restore system viewer password", function () {
        self.helper.restorePassword(self.helper.userEmailViewer);
    });
    it("restore system advanced viewer password", function () {
        self.helper.restorePassword(self.helper.userEmailAdvViewer);
    });
    it("restore system live viewer password", function () {
        self.helper.restorePassword(self.helper.userEmailLiveViewer);
    });
    it("restore password for user without system permissions", function () {
        self.helper.restorePassword(self.helper.userEmailNoPerm);
    });
    it("restore general user password", function () {
        self.helper.restorePassword(self.helper.userEmail);
    });
    it("restore second general user password", function () {
        self.helper.restorePassword(self.helper.userEmail2);
    });
});
