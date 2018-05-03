"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
var __metadata = (this && this.__metadata) || function (k, v) {
    if (typeof Reflect === "object" && typeof Reflect.metadata === "function") return Reflect.metadata(k, v);
};
var __param = (this && this.__param) || function (paramIndex, decorator) {
    return function (target, key) { decorator(target, key, paramIndex); }
};
Object.defineProperty(exports, "__esModule", { value: true });
const core_1 = require("@angular/core");
const ng_bootstrap_1 = require("@ng-bootstrap/ng-bootstrap");
const general_component_1 = require("../general/general.component");
let ShareModalContent = class ShareModalContent {
    constructor(activeModal, account, process, configService, toast, generalModal) {
        this.activeModal = activeModal;
        this.account = account;
        this.process = process;
        this.configService = configService;
        this.toast = toast;
        this.generalModal = generalModal;
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
        this.options = this.accessRoles;
    }
    formatUserName() {
        if (!this.user.fullName || this.user.fullName.trim() == '') {
            return this.user.email;
        }
        return this.user.fullName + ' (' + this.user.email + ')';
    }
    ;
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
        this.user = (this.user) ? Object.assign({}, this.user) : { email: '', isEnabled: true };
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
                    .openConfirm(this.language.sharing.confirmOwner, this.language.sharing.shareTitle, this.language.sharing.shareConfirmButton, null, this.language.dialogs.cancelButton)
                    .result
                    .then((result) => {
                    if (result === 'OK') {
                        this.doShare();
                    }
                });
            }
            else {
                return this.doShare();
            }
        }, {
            successMessage: this.language.sharing.permissionsSaved
        }).then(() => {
            this.activeModal.close();
        });
    }
};
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], ShareModalContent.prototype, "language", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], ShareModalContent.prototype, "system", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], ShareModalContent.prototype, "user", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], ShareModalContent.prototype, "closable", void 0);
ShareModalContent = __decorate([
    core_1.Component({
        selector: 'nx-modal-share-content',
        templateUrl: 'share.component.html',
        styleUrls: []
    }),
    __param(1, core_1.Inject('account')),
    __param(2, core_1.Inject('process')),
    __param(3, core_1.Inject('configService')),
    __param(4, core_1.Inject('ngToast')),
    __metadata("design:paramtypes", [ng_bootstrap_1.NgbActiveModal, Object, Object, Object, Object, general_component_1.NxModalGeneralComponent])
], ShareModalContent);
exports.ShareModalContent = ShareModalContent;
let NxModalShareComponent = class NxModalShareComponent {
    constructor(language, modalService) {
        this.language = language;
        this.modalService = modalService;
    }
    dialog(system, user) {
        this.modalRef = this.modalService.open(ShareModalContent, { backdrop: 'static' });
        this.modalRef.componentInstance.language = this.language.lang;
        this.modalRef.componentInstance.system = system;
        this.modalRef.componentInstance.user = user;
        this.modalRef.componentInstance.closable = true;
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
};
NxModalShareComponent = __decorate([
    core_1.Component({
        selector: 'nx-modal-share',
        template: '',
        encapsulation: core_1.ViewEncapsulation.None,
        styleUrls: []
    }),
    __param(0, core_1.Inject('languageService')),
    __metadata("design:paramtypes", [Object, ng_bootstrap_1.NgbModal])
], NxModalShareComponent);
exports.NxModalShareComponent = NxModalShareComponent;
//# sourceMappingURL=share.component.js.map