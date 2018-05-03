import { Component, Inject, OnInit, Input, ViewEncapsulation } from '@angular/core';
import { NgbModal, NgbActiveModal, NgbModalRef }               from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator }                                      from '@angular/forms';
import { NxModalGeneralComponent }                             from "../general/general.component";

@Component({
    selector: 'nx-modal-share-content',
    templateUrl: 'share.component.html',
    styleUrls: []
})
export class ShareModalContent {
    @Input() language;
    @Input() system;
    @Input() user;
    @Input() closable;

    title: string;
    sharing: any;
    url: string;
    accessRoles: any;
    options: any;
    isNewShare: boolean;
    buttonText: string;

    constructor(public activeModal: NgbActiveModal,
                @Inject('account') private account: any,
                @Inject('process') private process: any,
                @Inject('configService') private configService: any,
                @Inject('ngToast') private toast: any,
                private generalModal: NxModalGeneralComponent,) {

        this.url = 'share';
    }

    processAccessRoles() {
        const roles = this.system.accessRoles || this.configService.accessRoles.predefinedRoles;
        this.accessRoles = roles.filter((role) => {
            if (!(role.isOwner || role.isAdmin && !this.system.isMine)) {
                role.optionLabel = this.language.accessRoles[role.name].label || role.name;
                return role;
            }

            return false;
        });

        if (!this.user.role) {
            this.user.role = this.system.findAccessRole(this.user);
        }

        this.options = this.accessRoles
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
        this.title = (!this.user) ? this.language.sharing.shareTitle : this.language.sharing.editShareTitle;
        this.buttonText = this.language.sharing.shareConfirmButton;
        this.isNewShare = !this.user;

        this.user = (this.user) ? {...this.user} : {email:'', isEnabled: true};

        if (!this.isNewShare) {
            this.account
                .get()
                .then((account) => {
                    if (account.email == this.user.email) {
                        this.activeModal.close();
                        this.toast.create({
                            className: 'error',
                            content: this.language.share.cantEditYourself,
                            dismissOnTimeout: true,
                            dismissOnClick: true,
                            dismissButton: false
                        });
                    }
                });

            this.buttonText = this.language.sharing.editShareConfirmButton;
        }

        this.processAccessRoles();

        this.sharing = this.process.init(() => {
            if (this.user.role.isOwner) {
                return this.generalModal
                           .openConfirm(this.language.sharing.confirmOwner,
                               this.language.sharing.shareTitle,
                               this.language.sharing.shareConfirmButton,
                               null,
                               this.language.dialogs.cancelButton)
                           .result
                           .then((result) => {
                               if (result === 'OK') {
                                   this.doShare();
                               }
                           });
            } else {
                return this.doShare();
            }
        }, {
            successMessage: this.language.sharing.permissionsSaved
        }).then(() => {
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
                private modalService: NgbModal) {
    }

    private dialog(system?, user?) {
        this.modalRef = this.modalService.open(ShareModalContent, {backdrop: 'static'});
        this.modalRef.componentInstance.language = this.language.lang;
        this.modalRef.componentInstance.system = system;
        this.modalRef.componentInstance.user = user;
        this.modalRef.componentInstance.closable = true;

        return this.modalRef;
    }

    open(system?, user?) {
        return this.dialog(system, user);
    }

    close() {
        this.modalRef.close();
    }

    ngOnInit() {
    }
}
