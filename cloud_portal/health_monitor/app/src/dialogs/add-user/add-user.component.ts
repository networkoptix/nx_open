import {
    Component, Input, OnInit, Renderer2, ViewEncapsulation
}                                                from '@angular/core';
import { NgbActiveModal, NgbModal, NgbModalRef } from '@ng-bootstrap/ng-bootstrap';
import { NxConfigService }                       from '../../services/nx-config';
import { NxLanguageProviderService }             from '../../services/nx-language-provider';
import { NxModalGenericComponent }               from '../generic/generic.component';
import { NxToastService }                        from '../toast.service';
import { NxProcessService }                      from '../../services/process.service';
import { BehaviorSubject } from 'rxjs';

@Component({
    selector   : 'nx-modal-add-user-content',
    templateUrl: 'add-user.component.html',
    styleUrls  : []
})
export class AddUserModalContent {
    @Input() account;
    @Input() system;
    @Input() user;
    @Input() closable;

    LANG: any;
    CONFIG: any;

    title: string;
    sharing: any;
    url: string;
    accessRoles: any;
    options: any;
    isNewShare: boolean;
    buttonText: string;
    selectedPermissionSubject = new BehaviorSubject<any>({name: ''});
    accessDescription: string;

    constructor(public activeModal: NgbActiveModal,
                private renderer: Renderer2,
                private configService: NxConfigService,
                private genericModal: NxModalGenericComponent,
                private language: NxLanguageProviderService,
                private toastService: NxToastService,
                private processService: NxProcessService
    ) {
        this.url = 'share';
        this.accessRoles = [];
        this.CONFIG = this.configService.getConfig();
        this.LANG = this.language.getTranslations();
    }

    get selectedPermission() {
        return this.selectedPermissionSubject.getValue();
    }

    set selectedPermission(role) {
        this.user.role = role;
        this.selectedPermissionSubject.next(role);
    }

    private getRoleDescription() {
        if (this.selectedPermission.description) {
            return this.selectedPermission.description;
        }
        if (this.selectedPermission.userRoleId) {
            return this.LANG.accessRoles.customRole.description;
        }
        if (this.LANG.accessRoles[ this.selectedPermission.name ]) {
            return this.LANG.accessRoles[ this.selectedPermission.name ].description;
        }
        return this.LANG.accessRoles.customRole.description;
    }

    setPermission(role: any) {
        this.selectedPermission = role;
        this.accessDescription = this.LANG.accessRoles[this.selectedPermission.name] ?
                this.LANG.accessRoles[this.selectedPermission.name].description :
                this.LANG.accessRoles.customRole.description;
    }

    formatUserName() {
        if (!this.user.fullName || this.user.fullName.trim() === '') {
            return this.user.email;
        }

        return this.user.fullName + ' (' + this.user.email + ')';
    }

    doShare() {
        return this.system.saveUser(this.user, this.user.role).then((user) => {
            return this.system.getUsers(true).then(() => {
                return new Promise(resolve => setTimeout(() => resolve(user)));
            });
        });
    }

    ngOnInit() {
        this.title = (!this.user) ? this.LANG.dialogs.sharing.shareTitle : this.LANG.dialogs.sharing.editShareTitle;
        this.buttonText = this.LANG.dialogs.sharing.shareConfirmButton;
        this.isNewShare = false;

        if (!this.user) {
            this.isNewShare = true;
            this.user = {
                email    : '',
                isEnabled: true,
                role     : {
                    name       : this.CONFIG.accessRoles.default,
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
                    if (account) {
                        if (account.email === this.user.email) {
                            this.activeModal.close();

                            const options = {
                                autohide : true,
                                classname: 'error',
                                delay    : this.CONFIG.alertTimeout
                            };

                            return this.toastService.show(this.LANG.share.cantEditYourself, options);
                        }

                        this.accessDescription = this.getRoleDescription();
                    }
                });

            this.buttonText = this.LANG.sharing.editShareConfirmButton;
        }

        this.sharing = this.processService.createProcess(() => {
            if (this.user.role.isOwner) {
                return this.genericModal
                    .openConfirm(this.LANG.dialogs.sharing.confirmOwner,
                        this.LANG.dialogs.sharing.shareTitle,
                        this.LANG.dialogs.sharing.shareConfirmButton,
                        undefined,
                        this.LANG.dialogs.cancelButton)
                    .then((result) => {
                        if (result) {
                            return this.doShare();
                        }
                    });
            } else {
                return this.doShare();
            }
        }, {
            successMessage: this.LANG.dialogs.sharing.permissionsSaved,
            errorPrefix: this.LANG.errorCodes.cantSharePrefix
        }).then((user) => {
            this.activeModal.close(user.id);
        });
    }

    close() {
        this.activeModal.close();
    }
}

@Component({
    selector     : 'nx-modal-add-user',
    template     : '',
    encapsulation: ViewEncapsulation.None,
    styleUrls    : []
})
export class NxModalAddUserComponent implements OnInit {
    modalRef: NgbModalRef;

    constructor(private modalService: NgbModal) {
    }

    private dialog(system?, user?) {
        // TODO: Refactor dialog to use generic dialog
        // TODO: retire loading ModalContent (CLOUD-2493)
        this.modalRef = this.modalService.open(AddUserModalContent,
                {
                            windowClass: 'modal-holder',
                            backdrop: 'static'
                        });

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
