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
let DisconnectModalContent = class DisconnectModalContent {
    constructor(activeModal, account, process, cloudApi) {
        this.activeModal = activeModal;
        this.account = account;
        this.process = process;
        this.cloudApi = cloudApi;
    }
    ngOnInit() {
        this.disconnect = this.process.init(() => {
            return this.cloudApi.disconnect(this.systemId, this.password);
        }, {
            ignoreUnauthorized: true,
            errorCodes: {
                notAuthorized: this.language.errorCodes.passwordMismatch
            },
            successMessage: this.language.system.successDisconnected,
            errorPrefix: this.language.errorCodes.cantDisconnectSystemPrefix
        }).then(() => {
            this.activeModal.close('CLOSE');
        });
    }
    close() {
        this.activeModal.close('CLOSE');
    }
};
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], DisconnectModalContent.prototype, "systemId", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], DisconnectModalContent.prototype, "language", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], DisconnectModalContent.prototype, "disconnect", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], DisconnectModalContent.prototype, "closable", void 0);
DisconnectModalContent = __decorate([
    core_1.Component({
        selector: 'nx-modal-disconnect-content',
        templateUrl: 'disconnect.component.html',
        styleUrls: []
    }),
    __param(1, core_1.Inject('account')),
    __param(2, core_1.Inject('process')),
    __param(3, core_1.Inject('cloudApiService')),
    __metadata("design:paramtypes", [ng_bootstrap_1.NgbActiveModal, Object, Object, Object])
], DisconnectModalContent);
exports.DisconnectModalContent = DisconnectModalContent;
let NxModalDisconnectComponent = class NxModalDisconnectComponent {
    constructor(language, location, modalService) {
        this.language = language;
        this.location = location;
        this.modalService = modalService;
    }
    dialog(systemId) {
        this.modalRef = this.modalService.open(DisconnectModalContent);
        this.modalRef.componentInstance.language = this.language.lang;
        this.modalRef.componentInstance.disconnect = this.disconnect;
        this.modalRef.componentInstance.systemId = systemId;
        this.modalRef.componentInstance.closable = true;
        return this.modalRef;
    }
    open(systemId) {
        return this.dialog(systemId);
    }
    ngOnInit() {
    }
};
NxModalDisconnectComponent = __decorate([
    core_1.Component({
        selector: 'nx-modal-disconnect',
        template: '',
        encapsulation: core_1.ViewEncapsulation.None,
        styleUrls: []
    }),
    __param(0, core_1.Inject('languageService')),
    __metadata("design:paramtypes", [Object, common_1.Location,
        ng_bootstrap_1.NgbModal])
], NxModalDisconnectComponent);
exports.NxModalDisconnectComponent = NxModalDisconnectComponent;
//# sourceMappingURL=disconnect.component.js.map