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
let MergeModalContent = class MergeModalContent {
    constructor(activeModal, process, account, configService, systemsProvider, cloudApi) {
        this.activeModal = activeModal;
        this.process = process;
        this.account = account;
        this.configService = configService;
        this.systemsProvider = systemsProvider;
        this.cloudApi = cloudApi;
    }
    //Add system can merge where added to systems form api call
    checkMergeAbility(system) {
        if (system.stateOfHealth == 'offline') {
            return 'offline';
        }
        if (!system.canMerge) {
            return 'cannotMerge';
        }
        return '';
    }
    setTargetSystem(system) {
        this.targetSystem = system;
        this.systemMergeable = this.checkMergeAbility(system);
    }
    ;
    addStatus(system) {
        let status = "";
        if (system.stateOfHealth == 'offline') {
            status = ' (offline)';
        }
        else if (system.stateOfHealth == 'online' && system.canMerge) {
            status = ' (incompatable)';
        }
        return system.name + status;
    }
    ;
    ngOnInit() {
        this.account
            .get()
            .then((user) => {
            this.user = user;
            this.systems = this.systemsProvider.getMySystems(user.email, this.system.id);
            this.showMergeForm = this.system.canMerge && this.systems.length > 0;
            this.targetSystem = this.systems[0];
            this.systemMergeable = this.checkMergeAbility(this.targetSystem);
        });
        this.merging = this.process.init(() => {
            let masterSystemId = null;
            let slaveSystemId = null;
            if (this.masterId == this.system.id) {
                masterSystemId = this.system.id;
                slaveSystemId = this.targetSystem.id;
            }
            else {
                masterSystemId = this.targetSystem.id;
                slaveSystemId = this.system.id;
            }
            //return cloudApi.systems(); //In for testing purposes with merging things
            return this.cloudApi.merge(masterSystemId, slaveSystemId);
        }, {
            successMessage: this.language.system.mergeSystemSuccess
        }).then(() => {
            this.systemsProvider.forceUpdateSystems();
            this.activeModal.close({
                "anotherSystemId": this.targetSystem.id,
                "role": this.masterId == this.system.id ?
                    this.configService.systemStatuses.master :
                    this.configService.systemStatuses.slave
            });
        });
    }
};
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], MergeModalContent.prototype, "system", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], MergeModalContent.prototype, "systemName", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], MergeModalContent.prototype, "language", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], MergeModalContent.prototype, "closable", void 0);
MergeModalContent = __decorate([
    core_1.Component({
        selector: 'nx-modal-merge-content',
        templateUrl: 'merge.component.html',
        styleUrls: []
    }),
    __param(1, core_1.Inject('process')),
    __param(2, core_1.Inject('account')),
    __param(3, core_1.Inject('configService')),
    __param(4, core_1.Inject('systemsProvider')),
    __param(5, core_1.Inject('cloudApiService')),
    __metadata("design:paramtypes", [ng_bootstrap_1.NgbActiveModal, Object, Object, Object, Object, Object])
], MergeModalContent);
exports.MergeModalContent = MergeModalContent;
let NxModalMergeComponent = class NxModalMergeComponent {
    constructor(language, modalService) {
        this.language = language;
        this.modalService = modalService;
    }
    dialog(system) {
        this.modalRef = this.modalService.open(MergeModalContent);
        this.modalRef.componentInstance.language = this.language;
        this.modalRef.componentInstance.system = system;
        this.modalRef.componentInstance.closable = true;
        return this.modalRef;
    }
    open(system) {
        return this.dialog(system);
    }
    close() {
        this.modalRef.close();
    }
    ngOnInit() {
    }
};
NxModalMergeComponent = __decorate([
    core_1.Component({
        selector: 'nx-modal-merge',
        template: '',
        encapsulation: core_1.ViewEncapsulation.None,
        styleUrls: []
    }),
    __param(0, core_1.Inject('languageService')),
    __metadata("design:paramtypes", [Object, ng_bootstrap_1.NgbModal])
], NxModalMergeComponent);
exports.NxModalMergeComponent = NxModalMergeComponent;
//# sourceMappingURL=merge.component.js.map