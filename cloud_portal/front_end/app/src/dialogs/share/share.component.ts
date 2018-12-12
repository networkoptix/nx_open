import {
    Component,
    Inject,
    OnInit,
    Input,
    ViewEncapsulation,
    Renderer2,
    SimpleChanges,
    OnChanges
}                                                from '@angular/core';
import { NgbModal, NgbActiveModal, NgbModalRef } from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator }                        from '@angular/forms';
import { NxModalGenericComponent }               from '../generic/generic.component';
import { NxConfigService }                       from '../../services/nx-config';

@Component({
    selector   : 'nx-modal-share-content',
    templateUrl: 'share.component.html',
    styleUrls  : []
})
export class ShareModalContent {
    @Input() language;
    @Input() system;
    @Input() user;
    @Input() closable;

    config: any;
    title: string;
    sharing: any;
    url: string;
    accessRoles: any;
    options: any;
    isNewShare: boolean;
    buttonText: string;
    selectedPermission: {
        name: ''
    };
    accessDescription: string;

    constructor(public activeModal: NgbActiveModal,
                private renderer: Renderer2,
                private configService: NxConfigService,
                @Inject('account') private account: any,
                @Inject('process') private process: any,
                @Inject('ngToast') private toast: any,
                private genericModal: NxModalGenericComponent) {

        this.url = 'share';
        this.accessRoles = [];
        this.config = configService.getConfig();
    }


    private getRoleDescription() {
        if (this.user.role.description) {
            return this.user.role.description;
        }
        if (this.user.role.userRoleId) {
            return this.language.accessRoles.customRole.description;
        }
        if (this.language.accessRoles[ this.user.role.name ]) {
            return this.language.accessRoles[ this.user.role.name ].description;
        }
        return this.language.accessRoles.customRole.description;
    }

    setPermission(role: any) {
        this.selectedPermission = role;
        this.accessDescription = this.language.accessRoles[this.selectedPermission.name] ?
                this.language.accessRoles[this.selectedPermission.name].description :
                this.language.accessRoles.customRole.description;
    }

    formatUserName() {
        if (!this.user.fullName || this.user.fullName.trim() === '') {
            return this.user.email;
        }

        return this.user.fullName + ' (' + this.user.email + ')';
    }

    doShare() {
        this.user.role = this.selectedPermission;

        return this.system.saveUser(this.user, this.user.role);
    }

    ngOnInit() {
        this.title = (!this.user) ? this.language.sharing.shareTitle : this.language.sharing.editShareTitle;
        this.buttonText = this.language.sharing.shareConfirmButton;
        this.isNewShare = false;

        if (!this.user) {
            this.isNewShare = true;
            const predefinedRole = this.config.accessRoles.predefinedRoles.filter(role => {
                return role.name === this.config.accessRoles.default;
            })[0];
            this.user = {
                email    : '',
                isEnabled: true,
                role     : {
                    name       : this.config.accessRoles.default,
                    permissions: ''     // permissions will be updated within permissions component as it depends
                                        // on system's accessRoles
                }
            };
        }

        if (!this.user.role) {
            this.user.role = this.system.findAccessRole(this.user);
        }

        if (!this.isNewShare) {
            this.account
                .get()
                .then((account) => {
                    if (account.email === this.user.email) {
                        this.activeModal.close();
                        this.toast.create({
                            className       : 'error',
                            content         : this.language.share.cantEditYourself,
                            dismissOnTimeout: true,
                            dismissOnClick  : true,
                            dismissButton   : false
                        });
                    }

                    this.accessDescription = this.getRoleDescription();
                });

            this.buttonText = this.language.sharing.editShareConfirmButton;
        }

        this.sharing = this.process.init(() => {
            if (this.user.role.isOwner) {
                return this.genericModal
                    .openConfirm(this.language.sharing.confirmOwner,
                        this.language.sharing.shareTitle,
                        this.language.sharing.shareConfirmButton,
                        null,
                        this.language.dialogs.cancelButton)
                    .then((result) => {
                        if (result) {
                            this.doShare();
                        }
                    });
            } else {
                return this.doShare();
            }
        }, {
            successMessage: this.language.sharing.permissionsSaved
        }).then(() => {
            this.activeModal.close(true);
        });
    }

    close() {
        this.activeModal.close();
    }
}

@Component({
    selector     : 'nx-modal-share',
    template     : '',
    encapsulation: ViewEncapsulation.None,
    styleUrls    : []
})

export class NxModalShareComponent implements OnInit {
    modalRef: NgbModalRef;

    constructor(@Inject('languageService') private language: any,
                private modalService: NgbModal) {
    }

    private dialog(system?, user?) {
        // TODO: Refactor dialog to use generic dialog
        // TODO: retire loading ModalContent (CLOUD-2493)
        this.modalRef = this.modalService.open(ShareModalContent,
                {
                            windowClass: 'modal-holder',
                            backdrop: 'static'
                        });
        this.modalRef.componentInstance.language = this.language.lang;
        this.modalRef.componentInstance.system = system;
        this.modalRef.componentInstance.user = user;
        this.modalRef.componentInstance.closable = true;

        return this.modalRef;
    }

    open(system?, user?) {
        return this.dialog(system, user).result;
    }

    ngOnInit() {
    }
}
