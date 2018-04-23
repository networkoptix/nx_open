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
const login_component_1 = require("./../dialogs/login/login.component");
const general_component_1 = require("./../dialogs/general/general.component");
let nxDialogsService = class nxDialogsService {
    constructor(toast, loginModal, generalModal) {
        this.toast = toast;
        this.loginModal = loginModal;
        this.generalModal = generalModal;
    }
    dismiss() {
        this.toast.dismiss();
    }
    notify(message, type, hold) {
        type = type || 'info';
        return this.toast.create({
            className: type,
            content: message,
            dismissOnTimeout: !hold,
            dismissOnClick: !hold,
            dismissButton: hold
        });
    }
    confirm(message, title, actionLabel, actionType, cancelLabel) {
        //title, template, url, content, hasFooter, cancellable, params, closable, actionLabel, buttonType, size
        return this.generalModal.openConfirm(message, title, actionLabel, actionType, cancelLabel);
    }
    login(keepPage) {
        return this.loginModal.open(keepPage);
    }
};
nxDialogsService = __decorate([
    core_1.Injectable(),
    __param(0, core_1.Inject('ngToast')),
    __metadata("design:paramtypes", [Object, login_component_1.NxModalLoginComponent,
        general_component_1.NxModalGeneralComponent])
], nxDialogsService);
exports.nxDialogsService = nxDialogsService;
//# sourceMappingURL=dialogs.service.js.map