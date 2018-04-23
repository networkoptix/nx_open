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
const common_1 = require("@angular/common");
const ng_bootstrap_1 = require("@ng-bootstrap/ng-bootstrap");
const dialogs_service_1 = require("../dialogs.service");
let ShareModalContent = class ShareModalContent {
    constructor(activeModal, account, process, CONFIG, nxDialogs) {
        this.activeModal = activeModal;
        this.account = account;
        this.process = process;
        this.CONFIG = CONFIG;
        this.nxDialogs = nxDialogs;
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
                        .notify(this.language.sharing.confirmOwner, 'error');
                }
            });
            this.buttonText = this.language.sharing.editShareConfirmButton;
        }
        this.processAccessRoles();
        this.sharing = this.process.init(function () {
            if (this.user.role.isOwner) {
                return this.nxDialogs
                    .confirm(this.language.sharing.confirmOwner)
                    .then(this.doShare());
            }
            else {
                return this.doShare();
            }
        }, {
            successMessage: this.language.sharing.permissionsSaved
        }).then(function () {
            this.activeModal.close();
        });
    }
};
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
], ShareModalContent.prototype, "language", void 0);
ShareModalContent = __decorate([
    core_1.Component({
        selector: 'nx-modal-share-content',
        templateUrl: 'share.component.html',
        styleUrls: []
    }),
    __param(1, core_1.Inject('account')),
    __param(2, core_1.Inject('process')),
    __param(3, core_1.Inject('CONFIG')),
    __metadata("design:paramtypes", [ng_bootstrap_1.NgbActiveModal, Object, Object, Object, dialogs_service_1.nxDialogsService])
], ShareModalContent);
exports.ShareModalContent = ShareModalContent;
let NxModalShareComponent = class NxModalShareComponent {
    constructor(language, location, modalService) {
        this.language = language;
        this.location = location;
        this.modalService = modalService;
    }
    dialog(system, user) {
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
};
NxModalShareComponent = __decorate([
    core_1.Component({
        selector: 'nx-modal-share',
        template: '',
        encapsulation: core_1.ViewEncapsulation.None,
        styleUrls: []
    }),
    __param(0, core_1.Inject('languageService')),
    __metadata("design:paramtypes", [Object, common_1.Location,
        ng_bootstrap_1.NgbModal])
], NxModalShareComponent);
exports.NxModalShareComponent = NxModalShareComponent;
//# sourceMappingURL=share.component.js.map