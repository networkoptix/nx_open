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
let RenameModalContent = class RenameModalContent {
    constructor(activeModal, process, cloudApi) {
        this.activeModal = activeModal;
        this.process = process;
        this.cloudApi = cloudApi;
    }
    ngOnInit() {
        this.renaming = this.process.init(function () {
            return this.cloudApi.renameSystem(this.systemId, this.systemName);
        }, {
            successMessage: this.language.system.successRename
        }).then(function () {
            this.activeModal.close();
        });
    }
};
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], RenameModalContent.prototype, "systemId", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], RenameModalContent.prototype, "systemName", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], RenameModalContent.prototype, "language", void 0);
RenameModalContent = __decorate([
    core_1.Component({
        selector: 'nx-modal-rename-content',
        templateUrl: 'rename.component.html',
        styleUrls: []
    }),
    __param(1, core_1.Inject('process')),
    __param(2, core_1.Inject('cloudApiService')),
    __metadata("design:paramtypes", [ng_bootstrap_1.NgbActiveModal, Object, Object])
], RenameModalContent);
exports.RenameModalContent = RenameModalContent;
let NxModalRenameComponent = class NxModalRenameComponent {
    constructor(language, modalService) {
        this.language = language;
        this.modalService = modalService;
    }
    dialog(systemId, systemName) {
        this.modalRef = this.modalService.open(RenameModalContent);
        this.modalRef.componentInstance.language = this.language;
        this.modalRef.componentInstance.systemId = systemId;
        this.modalRef.componentInstance.systemName = systemName;
        return this.modalRef;
    }
    open(systemId, systemName) {
        return this.dialog(systemId, systemName);
    }
    close() {
        this.modalRef.close();
    }
    ngOnInit() {
    }
};
NxModalRenameComponent = __decorate([
    core_1.Component({
        selector: 'nx-modal-rename',
        template: '',
        encapsulation: core_1.ViewEncapsulation.None,
        styleUrls: []
    }),
    __param(0, core_1.Inject('languageService')),
    __metadata("design:paramtypes", [Object, ng_bootstrap_1.NgbModal])
], NxModalRenameComponent);
exports.NxModalRenameComponent = NxModalRenameComponent;
//# sourceMappingURL=rename.component.js.map