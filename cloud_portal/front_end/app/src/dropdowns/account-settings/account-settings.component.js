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
let NxAccountSettingsDropdown = class NxAccountSettingsDropdown {
    constructor(account) {
        this.account = account;
        this.settings = {
            email: '',
            is_staff: false
        };
    }
    ngOnInit() {
        this.account
            .get()
            .then(result => {
            this.settings.email = result.email;
            this.settings.is_staff = result.is_staff;
        });
    }
    logout() {
        this.account.logout();
    }
};
NxAccountSettingsDropdown = __decorate([
    core_1.Component({
        selector: 'nx-account-settings-select',
        templateUrl: 'account-settings.component.html',
        styleUrls: ['account-settings.component.scss']
    }),
    __param(0, core_1.Inject('account')),
    __metadata("design:paramtypes", [Object])
], NxAccountSettingsDropdown);
exports.NxAccountSettingsDropdown = NxAccountSettingsDropdown;
//# sourceMappingURL=account-settings.component.js.map