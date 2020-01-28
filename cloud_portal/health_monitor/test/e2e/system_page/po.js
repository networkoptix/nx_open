'use strict';

var SystemPage = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();
    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();

    var p = this;

    this.url = '/systems';
    this.systemLink = p.helper.systemLink;
    this.systemName = p.helper.systemName;

    this.deleteConfirmations = {
        self: 'You are going to disconnect this system from your account. You will lose an access for this system. Are you sure?',
        other: ''
    };

    this.roleHints = {
        admin: 'Unrestricted access including the ability to share',
        viewer: 'Can view live video and browse the archive',
        advViewer: 'Can view live video, browse the archive, configure cameras, control PTZ etc',
        liveViewer: 'Can only view live video'
    };

    //this.systemsList = element.all(by.repeater('system in systems'));
    this.systemsList = this.helper.allSystems;
    this.secondSystem = element.all(by.repeater('system in systems')).get(1);
    this.ownedSystem = element.all(by.cssContainingText('h2', this.systemName)).first();

    this.systemNameElem = element(by.css('h1'));
    this.systemOwnElem = element.all(by.css('.panel')).get(0).all(by.css('h2,h3'));
    this.openInClientButton = element(by.partialButtonText('Open in '));
    this.shareButton = element(by.partialButtonText('Share'));
    this.emailField = element(by.model('user.email'));
    this.roleField = element(by.model('user.role'));
    this.roleOptionAdmin = this.roleField.element(by.cssContainingText('option', 'Administrator'));
    this.roleHintBlock = element(by.css('span.help-block'));
    this.submitShareButton = element(by.css('process-button')).element(by.buttonText('Share'));
    this.ownerDeleteButton = element(by.cssContainingText('button','Disconnect from'));
    this.userDeleteButton = element(by.cssContainingText('button','Disconnect from my account'));
    this.disconnectDialog = element(by.css('.modal-dialog'));
    this.cancelDisconnectButton = this.disconnectDialog.element(by.buttonText('Cancel'));
    this.confirmDeleteButton = this.disconnectDialog.element(by.buttonText('Delete'));
    this.disconnectPasswordField = this.disconnectDialog.element(by.css('#password'));
    this.confirmDisconnectButton = this.disconnectDialog.element(by.buttonText('Disconnect'));


    this.selectRoleOption = function(role) {
        return this.helper.forms.share.roleField.element(by.css('option[label="' + role + '"]'));
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

    this.loginButton = element(by.css('.modal-dialog')).element(by.buttonText('Log in'));
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