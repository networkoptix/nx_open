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
Object.defineProperty(exports, "__esModule", { value: true });
const core_1 = require("@angular/core");
let NxAccountSettingsDropdown = class NxAccountSettingsDropdown {
    constructor() {
    }
    ngOnInit() {
        // prevent *ngIf complaining of undefined property --TT
        if (undefined === this.account) {
            this.account = {
                is_staff: false
            };
        }
    }
};
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], NxAccountSettingsDropdown.prototype, "account", void 0);
NxAccountSettingsDropdown = __decorate([
    core_1.Component({
        selector: 'nx-account-settings-select',
        templateUrl: './dropdown/account-settings/account-settings.component.html',
        styleUrls: ['./dropdown/account-settings/account-settings.component.scss']
    }),
    __metadata("design:paramtypes", [])
], NxAccountSettingsDropdown);
exports.NxAccountSettingsDropdown = NxAccountSettingsDropdown;
//# sourceMappingURL=account-settings.component.js.map