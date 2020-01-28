'use strict';
var SystemPage = require('./po.js');
describe('Sharing.', function () {

    var p = new SystemPage();

    afterEach(function(){
        p.helper.logout();
    });

    it ("Edit permissions for user opens share dialog with locked email and visible user name", function() {
        var viewerEmail = p.helper.userEmailViewer;
        var newName = 'visible users name';
        p.helper.login(viewerEmail);
        p.helper.changeAccountNames(newName, newName);
        p.helper.logout();

        p.helper.loginToSystems(p.helper.userEmailOwner);
        p.ownedSystem.click();
        p.usrDataRow(viewerEmail).click();
        p.usrDataRow(viewerEmail).element(by.css('.glyphicon-pencil')).click();
        expect(p.permEmailFieldDisabled.getAttribute('value')).toContain(viewerEmail);
        expect(p.permEmailFieldDisabled.getAttribute('value')).toContain(newName);
        p.permEmailFieldDisabled.sendKeys('attempt to edit');
        expect(p.permEmailFieldDisabled.getAttribute('value')).not.toContain('attempt to edit');
        p.permDialogClose.click();
    });

    it ("Share button - opens dialog", function() {
        p.helper.loginToSystems(p.helper.userEmailOwner);
        p.ownedSystem.click();
        p.shareButton.click();

        expect(p.permDialog.isPresent()).toBe(true);
        expect(p.permEmailFieldEnabled.isDisplayed()).toBe(true);
        p.permDialogClose.click();
    });

    it ("Sharing link /systems/{system_id}/share - opens dialog", function() {
        p.helper.loginToSystems(p.helper.userEmailOwner);

        p.helper.get(p.helper.urls.systems + p.systemLink +'/share');
        expect(p.permDialog.isPresent()).toBe(true);
        expect(p.permEmailFieldEnabled.isDisplayed()).toBe(true);
        p.permDialogClose.click();
    });

    it ("Sharing link for anonymous - first ask login, then show share dialog", function() {
        p.helper.get(p.helper.urls.systems + p.systemLink +'/share');
        expect(p.helper.forms.login.dialog.isDisplayed()).toBe(true);
        //expect(p.permDialog.isDisplayed()).toBe(false);
        p.helper.loginFromCurrPage(p.helper.userEmailOwner);
        expect(p.permDialog.isPresent()).toBe(true);
        p.permDialogClose.click();
    });

    it ("After closing dialog, called by link - clear link", function() {
        p.helper.loginToSystems(p.helper.userEmailOwner);

        p.helper.get(p.helper.urls.systems + p.systemLink +'/share');
        expect(p.permDialog.isPresent()).toBe(true);
        expect(p.permEmailFieldEnabled.isDisplayed()).toBe(true);
        p.permDialogClose.click();
        expect(browser.getCurrentUrl()).not.toContain(p.systemLink +'/share');
    });

    it ("Sharing roles are ordered: more access is on top of the list with options", function() {
        p.helper.loginToSystems(p.helper.userEmailOwner);

        p.helper.get(p.helper.urls.systems + p.systemLink +'/share');
        expect(p.permDialog.isPresent()).toBe(true);
        expect(p.permEmailFieldEnabled.isDisplayed()).toBe(true);
        expect(p.roleField.getText()).toMatch('Administrator\nAdvanced Viewer\nViewer\nLive Viewer\nCustom');
        p.permDialogClose.click();
    });

    it ("When user selects role - special hint appears", function() {
        p.helper.loginToSystems(p.helper.userEmailOwner);

        p.helper.get(p.helper.urls.systems + p.systemLink +'/share');
        expect(p.permDialog.isPresent()).toBe(true);
        expect(p.permEmailFieldEnabled.isDisplayed()).toBe(true);
        p.roleField.click();
        p.selectRoleOption(p.helper.roles.admin).click();
        expect(p.roleHintBlock.getText()).toContain(p.roleHints.admin);
        p.selectRoleOption(p.helper.roles.viewer).click();
        expect(p.roleHintBlock.getText()).toContain(p.roleHints.viewer);
        p.selectRoleOption(p.helper.roles.advViewer).click();
        expect(p.roleHintBlock.getText()).toContain(p.roleHints.advViewer);
        p.selectRoleOption(p.helper.roles.liveViewer).click();
        expect(p.roleHintBlock.getText()).toContain(p.roleHints.liveViewer);
        p.permDialogClose.click();
    });

    //xit ("Not owner can't change owner", function() {
    //});
    //xit ("Owner can change owner", function() {
    //    p.helper.login(p.helper.userEmailOwner);
    //
    //    p.helper.get(p.helper.urls.systems + p.systemLink +'/share');
    //    expect(p.permDialog.isPresent()).toBe(true);
    //    expect(p.permEmailFieldEnabled.isDisplayed()).toBe(true);
    //    browser.pause();
    //    p.roleField.click();
    //    p.permDialogClose.click();
    //});
    //xit ("If user changes owner - he should confirm it", function() {
    //});

    //xit ("User cant edit his own permissions", function() {
    //}); // This should work, but doesn't

    //xit ("User cant edit his own permissions by direct sharing link", function() {
    //    p.helper.login(p.helper.userEmailViewer);
    //
    //    p.helper.get(p.helper.urls.systems + p.systemLink +'/share');
    //    expect(p.permDialog.isPresent()).toBe(true);
    //    expect(p.permEmailFieldEnabled.isDisplayed()).toBe(true);
    //    p.permEmailFieldEnabled.sendKeys(p.helper.userEmailViewer);
    //    p.roleField.click();
    //    p.selectRoleOption(p.helper.roles.admin).click();
    //    p.submitShareButton.click();
    //});

    xit ("Edit permission works, currently failing due to CLOUD-1596", function() {
        p.helper.loginToSystems(p.helper.userEmailOwner);
        p.ownedSystem.click();
        // Change permissions of viewer to live viewer
        p.usrDataRow(p.helper.userEmailViewer).click();
        p.usrDataRow(p.helper.userEmailViewer).element(by.css('.glyphicon-pencil')).click();
        p.roleField.click();
        p.selectRoleOption(p.helper.roles.liveViewer).click();
        p.permDialogSubmit.click();
        p.alert.catchAlert(p.alert.alertMessages.permissionAddSuccess, p.alert.alertTypes.success);
        expect(p.usrDataRow(p.helper.userEmailViewer).getText()).toContain('Live Viewer');
        // Change permissions of live viewer to viewer back
        p.usrDataRow(p.helper.userEmailViewer).click();
        p.usrDataRow(p.helper.userEmailViewer).element(by.css('.glyphicon-pencil')).click();
        p.roleField.click();
        p.selectRoleOption(p.helper.roles.viewer).click();
        p.permDialogSubmit.click();
        p.alert.catchAlert(p.alert.alertMessages.permissionAddSuccess, p.alert.alertTypes.success);
        expect(p.usrDataRow(p.helper.userEmailViewer).getText()).not.toContain('Live Viewer');
    });

    xit ("Sharing works, , currently failing due to CLOUD-1596", function() {
        var newUserEmail = p.helper.getRandomEmail();
        p.helper.createUser('Mark', 'Hamill', newUserEmail);

        p.helper.loginToSystems(p.helper.userEmailOwner);
        p.helper.get(p.helper.urls.systems + p.systemLink +'/share');

        p.emailField.sendKeys(newUserEmail);
        p.roleField.click();
        p.selectRoleOption(p.helper.roles.viewer).click();
        p.submitShareButton.click();
        p.alert.catchAlert(p.alert.alertMessages.permissionAddSuccess, p.alert.alertTypes.success);
        p.modalDialog.isPresent().then(function(isPresent) {
            if(isPresent) {
                p.modalDialogClose.click();
                expect('Share dialog').toBe('absent');
            }
        });
        expect(p.usrDataRow(newUserEmail).getText()).toContain('Viewer');
    });

    xit ("Share with registered user - sends him notification", function() {
        p.helper.createUser('Mark', 'Hamill').then( function(userEmail) {
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);

            p.helper.loginToSystems(p.helper.userEmailOwner, p.helper.userPassword);
            p.helper.getSysPage(p.systemLink);
            p.shareButton.click();
            p.emailField.sendKeys(userEmail);
            p.roleField.click();
            p.selectRoleOption(p.helper.roles.liveViewer).click();
            p.submitShareButton.click();
            p.alert.catchAlert(p.alert.alertMessages.permissionAddSuccess, p.alert.alertTypes.success);

            expect(p.usrDataRow(userEmail).isPresent()).toBe(true);

            //browser.controlFlow().wait(p.helper.getEmailTo(userEmail, p.helper.emailSubjects.share).then(function(mail) {
                //console.log(mail.headers.to);
            //}));
        });
    });

    it ("When edit share: Email label in dialog replaced with User", function() {
        p.helper.loginToSystems(p.helper.userEmailOwner);

        p.helper.get(p.helper.urls.systems + p.systemLink +'/share');
        expect(p.permDialog.isPresent()).toBe(true);
        expect(p.helper.getParentOf(p.permEmailFieldEnabled).getText()).toContain('Email');
        p.permDialogCancel.click();

        p.usrDataRow(p.helper.userEmailViewer).click();
        p.usrDataRow(p.helper.userEmailViewer).element(by.css('.glyphicon-pencil')).click();
        expect(p.helper.getParentOf(p.permEmailFieldEnabled).getText()).toContain('User');
        p.permDialogClose.click();
    });

    it ("Sharing dialog has cancel button, which works", function() {
        p.helper.loginToSystems(p.helper.userEmailOwner);

        p.helper.get(p.helper.urls.systems + p.systemLink +'/share');
        expect(p.permDialog.isPresent()).toBe(true);
        expect(p.permEmailFieldEnabled.isDisplayed()).toBe(true);
        p.permDialogCancel.click();
    });
});