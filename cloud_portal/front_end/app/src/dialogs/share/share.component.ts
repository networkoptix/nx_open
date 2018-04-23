import { Component, Inject, OnInit, Input, ViewEncapsulation } from '@angular/core';
import { Location }                                            from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef }               from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator }                                      from '@angular/forms';
import { nxDialogsService }                                    from "../dialogs.service";

@Component({
    selector: 'nx-modal-share-content',
    templateUrl: 'share.component.html',
    styleUrls: []
})
export class ShareModalContent {
    @Input() system;
    @Input() user;
    @Input() language;

    title: string;
    sharing: any;
    url: string;
    accessRoles: any;
    isNewShare: boolean;
    buttonText: string;

    constructor(public activeModal: NgbActiveModal,
                @Inject('account') private account: any,
                @Inject('process') private process: any,
                @Inject('CONFIG') private CONFIG: any,
                private nxDialogs: nxDialogsService) {

        this.url = 'share';
        this.title = (!this.user) ? this.language.sharing.shareTitle : this.language.sharing.editShareTitle;
    }

    processAccessRoles() {
        const roles = this.system.accessRoles || this.CONFIG.accessRoles.predefinedRoles;
        this.accessRoles = roles.filter((role) => {
            return !(role.isOwner || role.isAdmin && !this.system.isMine);
        });

        if (!this.user.role) {
            this.user.role = this.system.findAccessRole(this.user);
        }
    }

    formatUserName() {
        if (!this.user.fullName || this.user.fullName.trim() == '') {
            return this.user.email;
        }

        return this.user.fullName + ' (' + this.user.email + ')';
    };

    doShare() {
        return this.system.saveUser(this.user, this.user.role);
    }

    getRoleDescription() {
        if (this.user.role.description) {
            return this.user.role.description;
        }
        if (this.user.role.userRoleId) {
            return this.language.accessRoles.customRole.description;
        }
        if (this.language.accessRoles[this.user.role.name]) {
            return this.language.accessRoles[this.user.role.name].description;
        }
        return this.language.accessRoles.customRole.description;
    }

    ngOnInit() {

        // var dialogSettings = dialogs.getSettings($scope);
        // var system = dialogSettings.params.system;

        this.buttonText = this.language.sharing.shareConfirmButton;

        const systemId = this.system.id;
        this.isNewShare = !this.user;

        if (!this.isNewShare) {
            this.account
                .get()
                .then((account) => {
                    if (account.email == this.user.email) {
                        //$scope.$parent.cancel(this.language.share.cantEditYourself);
                        this.activeModal.close();
                        this.nxDialogs
                            .notify(
                                this.language.sharing.confirmOwner, 'error')
                    }
                });

            this.buttonText = this.language.sharing.editShareConfirmButton;
        }

        this.processAccessRoles();

        this.sharing = this.process.init(function () {
            if (this.user.role.isOwner) {
                return this.nxDialogs
                    .confirm(
                        this.language.sharing.confirmOwner)
                    .then(this.doShare());
            } else {
                return this.doShare();
            }
        }, {
            successMessage: this.language.sharing.permissionsSaved
        }).then(function () {
            this.activeModal.close();
        });
    }
}

@Component({
    selector: 'nx-modal-share',
    template: '',
    encapsulation: ViewEncapsulation.None,
    styleUrls: []
})

export class NxModalShareComponent implements OnInit {
    modalRef: NgbModalRef;

    constructor(@Inject('languageService') private language: any,
                private location: Location,
                private modalService: NgbModal) {
    }

    private dialog(system, user) {
        this.modalRef = this.modalService.open(ShareModalContent);
        this.modalRef.componentInstance.language = this.language;
        this.modalRef.componentInstance.system = system;
        this.modalRef.componentInstance.user = user;

        return this.modalRef;
    }

    open(system, user) {
        return this.dialog(system, user);
    }

    close() {
        this.modalRef.close();
    }

    ngOnInit() {
    }
}
