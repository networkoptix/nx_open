'use strict';
var SystemPage = require('./po.js');
describe('User list', function () {

    var p = new SystemPage();

    beforeEach(function(){
        p.helper.loginToSystems(p.helper.userEmailOwner, p.helper.userPassword);
    });

    afterEach(function(){
        p.helper.logout();
    });

    it("Shares system with other users", function() {
        p.helper.openSystemByLink();
        p.helper.shareSystemWith(p.helper.userEmailAdmin, p.helper.roles.admin);
        p.helper.shareSystemWith(p.helper.userEmailViewer, p.helper.roles.viewer);
        p.helper.shareSystemWith(p.helper.userEmailAdvViewer, p.helper.roles.advViewer);
        p.helper.shareSystemWith(p.helper.userEmailLiveViewer, p.helper.roles.liveViewer);
    });

    it ("is visible for owner and admin", function() {
        p.ownedSystem.click();
        expect(p.userList.isDisplayed()).toBe(true);
    
        p.helper.logout();
        p.helper.loginToSystems(p.helper.userEmailAdmin, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.userList.isDisplayed()).toBe(true);
    });
    
    it ("is not visible for other users", function() {
        p.helper.logout();
        p.helper.loginToSystems(p.helper.userEmailViewer, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.users.count()).toBe(0);
    
        p.helper.logout();
        p.helper.loginToSystems(p.helper.userEmailLiveViewer, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.users.count()).toBe(0);
    
        p.helper.logout();
        p.helper.loginToSystems(p.helper.userEmailAdvViewer, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.users.count()).toBe(0);
    });
    
    it ("contains user emails and names", function() {
        p.ownedSystem.click();
        expect(p.userList.getText()).toContain(p.helper.userEmailAdmin, p.helper.userFirstName);
    });
    
    it ("always displays owner on the top of the table", function() {
        p.ownedSystem.click();
        expect(p.users.first().getText()).toContain(p.helper.userEmailOwner);
    });
    
    it ("displays pencil and cross links for each user only on hover", function() {
        p.helper.logout();
        p.helper.loginToSystems(p.helper.userEmailAdmin);
        p.ownedSystem.click();
        p.users.filter(function(elem, index) {
            // Except owner, admins and current user
            return elem.getText().then(function(text) {
                return (!p.helper.isSubstr(text, p.helper.roles.admin)) &&
                    (!p.helper.isSubstr(text, p.helper.roles.owner)) &&
                    (!p.helper.isSubstr(text, p.helper.userEmailAdmin));
            });
        }).each(function(elem){
            expect(elem.element(by.css('.glyphicon-pencil')).isDisplayed()).toBe(false);
            expect(elem.element(by.css('.glyphicon-remove')).isDisplayed()).toBe(false);
            elem.click(); // TODO: replace with hover
            expect(elem.element(by.css('.glyphicon-pencil')).isDisplayed()).toBe(true);
            expect(elem.element(by.css('.glyphicon-remove')).isDisplayed()).toBe(true);
        });
    });
    
    it ("does not display edit and remove for owner row", function() {
        p.helper.logout();
        p.helper.loginToSystems(p.helper.userEmailAdmin);
        p.ownedSystem.click();
        p.users.first().click(); // TODO: replace with hover
        expect(p.users.first().element(by.css('.glyphicon-pencil')).isPresent()).toBe(false);
        expect(p.users.first().element(by.css('.glyphicon-remove')).isPresent()).toBe(false);
    });
    
    it ("removes user with confirmation, after deleting another user - message appears, user list is updated", function() {
        p.helper.logout();
        p.helper.loginToSystems(p.helper.userEmailAdmin);
        p.ownedSystem.click();
        var firstDeletableUser =  p.users.filter(function(elem, index) {
            // Except owner and current user
            return elem.getText().then(function(text) {
                return (!p.helper.isSubstr(text, p.helper.roles.admin)) &&
                    (!p.helper.isSubstr(text, p.helper.roles.owner)) &&
                    (!p.helper.isSubstr(text, p.helper.userEmailAdmin));
            });
        }).first();
    
        firstDeletableUser.getText().then(function(text) {
            firstDeletableUser.click(); // TODO: replace with hover
            expect(firstDeletableUser.element(by.css('.glyphicon-remove')).isDisplayed()).toBe(true);
            firstDeletableUser.element(by.css('.glyphicon-remove')).click();
            expect(p.modalDialog.getText()).toContain(p.deleteConfirmations.other);
            p.deleteUserButton.click();
            p.alert.catchAlert(p.alert.alertMessages.permissionDeleteSuccess, p.alert.alertTypes.success);
            expect(p.userList.getText()).not.toContain(text);
        });
    });
    
    // it ("asks for confirmation, if user deletes himself from the table", function() {
    //     p.helper.logout();
    //     p.helper.login(p.helper.userEmailAdmin);
    //     p.ownedSystem.click();
    //     var currentUsrRow = p.usrDataRow(p.helper.userEmailAdmin); // row with current user
    //     currentUsrRow.click(); // TODO: replace with hover
    //     currentUsrRow.element(by.css('.glyphicon-remove')).click();
    //     expect(p.modalDialog.getText()).toContain(p.deleteConfirmations.self);
    //     p.cancelDelUserButton.click();
    // });

    //xit ("If user deletes himself - he jumps to systems list", function() {
    //
    //});
    
    it ("updates after sharing", function() {
        p.helper.logout();
        p.helper.createUser(p.helper.userNameCyrillic, p.helper.userNameCyrillic).then( function(userEmail) {
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
    
            p.helper.loginToSystems(p.helper.userEmailOwner, p.helper.userPassword);
            p.helper.getSysPage(p.systemLink);
            p.shareButton.click();
            p.emailField.sendKeys(userEmail);
            p.roleField.click();
            p.roleOptionAdmin.click();
            p.submitShareButton.click();
            p.alert.catchAlert(p.alert.alertMessages.permissionAddSuccess, p.alert.alertTypes.success);
    
            expect(p.usrDataRow(userEmail).isPresent()).toBe(true);
        });
    });
});