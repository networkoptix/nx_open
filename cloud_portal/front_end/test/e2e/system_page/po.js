'use strict';

var SystemPage = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();
    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();

    this.url = '/#/systems';
    this.systemLink = '/a74840da-f135-4522-abd9-5a8c6fb8591f';
    this.systemName = 'test-korneeva';

    this.deleteConfirmations = {
        self: 'You are going to disconnect this system from your account. You will lose an access for this system. Are you sure?',
        other: ''
    };

    this.roleHints = {
        admin: 'Can configure system and share it',
        viewer: 'Can view live video from cameras and archive',
        advViewer: 'Can view live video from cameras and archive, configure cameras',
        liveViewer: 'Can view only live video from cameras'
    };

    this.systemsList = element.all(by.repeater('system in systems'));
    this.ownedSystem = element.all(by.cssContainingText('h2', this.systemName)).first();

    this.systemNameElem = element(by.css('h1'));
    this.systemOwnElem = element.all(by.css('.panel')).get(0).all(by.css('h2,h3'));
    this.openInClientButton = element(by.partialButtonText('Open in '));
    this.shareButton = element(by.partialButtonText('Share'));
    this.emailField = element(by.model('share.accountEmail'));
    this.roleField = element(by.model('share.accessRole'));
    this.roleOptionAdmin = this.roleField.element(by.css('option[label=admin]'));
    this.roleHintBlock = element(by.css('span.help-block'));
    this.submitShareButton = element(by.css('process-button')).element(by.buttonText('Share'));
    this.ownerDeleteButton = element(by.cssContainingText('button','Disconnect from'));
    this.userDeleteButton = element(by.cssContainingText('button','Remove from my Account'));
    this.disconnectDialog = element(by.css('.modal-dialog'));
    this.cancelDisconnectButton = this.disconnectDialog.element(by.buttonText('Cancel'));

    this.selectRoleOption = function(role) {
        return this.roleField.element(by.css(role));
    };

    this.users = element.all(by.repeater('user in system.users'));
    this.userList = this.helper.getParentOf(this.users.first());

    this.modalDialog = element(by.css('.modal-dialog'));
    this.deleteUserButton = this.modalDialog.element(by.buttonText('Delete'));
    this.cancelDelUserButton = this.modalDialog.element(by.buttonText('Cancel'));
    this.modalDialogClose = this.modalDialog.element(by.css('.close'));

    this.usrDataRow = function(email) {
        return this.helper.getParentOf(element(by.cssContainingText('td', email)));
    };

    this.loginButton = element(by.css('.modal-dialog')).element(by.buttonText('Login'));
    this.loginCloseButton = element(by.css('.modal-dialog')).all(by.css('button.close')).first();

    this.permDialog = element(by.css('.modal-dialog'));
    this.permEmailFieldDisabled = this.permDialog.element(by.css('[name=email][readonly]'));
    this.permEmailFieldEnabled = this.permDialog.element(by.css('[name=email]'));
    this.permRoleField = this.permDialog.element(by.model('share.accessRole'));
    this.permDialogSubmit = this.permDialog.element(by.buttonText('Save'));
    this.permDialogCancel = this.permDialog.element(by.buttonText('Cancel'));
    this.permDialogClose = this.permDialog.element(by.css('.close'));

};

module.exports = SystemPage;