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
let GeneralModalContent = class GeneralModalContent {
    constructor(activeModal) {
        this.activeModal = activeModal;
    }
};
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], GeneralModalContent.prototype, "message", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], GeneralModalContent.prototype, "title", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], GeneralModalContent.prototype, "actionLabel", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], GeneralModalContent.prototype, "actionType", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], GeneralModalContent.prototype, "cancelLabel", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], GeneralModalContent.prototype, "hasFooter", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], GeneralModalContent.prototype, "cancellable", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], GeneralModalContent.prototype, "closable", void 0);
GeneralModalContent = __decorate([
    core_1.Component({
        selector: 'nx-modal-general-content',
        templateUrl: 'general.component.html',
        styleUrls: []
        // TODO: later
        // templateUrl: this.CONFIG.viewsDir + 'components/dialog.html'
    }),
    __metadata("design:paramtypes", [ng_bootstrap_1.NgbActiveModal])
], GeneralModalContent);
exports.GeneralModalContent = GeneralModalContent;
let NxModalGeneralComponent = class NxModalGeneralComponent {
    constructor(language, location, modalService) {
        this.language = language;
        this.location = location;
        this.modalService = modalService;
    }
    dialog(message, title, actionLabel, actionType, cancelLabel, hasFooter, cancellable, closable, params) {
        this.modalRef = this.modalService.open(GeneralModalContent);
        this.modalRef.componentInstance.language = this.language;
        this.modalRef.componentInstance.message = message;
        this.modalRef.componentInstance.title = title;
        this.modalRef.componentInstance.actionLabel = actionLabel;
        this.modalRef.componentInstance.buttonType = actionType;
        this.modalRef.componentInstance.cancelLabel = cancelLabel;
        this.modalRef.componentInstance.buttonClass = actionType;
        this.modalRef.componentInstance.hasFooter = hasFooter;
        this.modalRef.componentInstance.cancellable = cancellable;
        this.modalRef.componentInstance.closable = closable;
        return this.modalRef;
    }
    openConfirm(message, title, actionLabel, actionType, cancelLabel) {
        return this.dialog(message, title, actionLabel, actionType, cancelLabel, true, false, false);
    }
    openAlert(message, title) {
        return this.dialog(message, title, this.language.lang.dialogs.okButton, null, this.language.lang.dialogs.cancelButton, true, true, true);
    }
    close() {
        this.modalRef.close();
    }
    ngOnInit() {
    }
};
NxModalGeneralComponent = __decorate([
    core_1.Component({
        selector: 'nx-modal-general',
        template: '',
        encapsulation: core_1.ViewEncapsulation.None,
        styleUrls: []
    }),
    __param(0, core_1.Inject('languageService')),
    __metadata("design:paramtypes", [Object, common_1.Location,
        ng_bootstrap_1.NgbModal])
], NxModalGeneralComponent);
exports.NxModalGeneralComponent = NxModalGeneralComponent;
//# sourceMappingURL=general.component.js.map