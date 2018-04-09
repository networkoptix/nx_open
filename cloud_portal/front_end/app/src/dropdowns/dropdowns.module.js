"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", { value: true });
const core_1 = require("@angular/core");
const common_1 = require("@angular/common");
const ng_bootstrap_1 = require("@ng-bootstrap/ng-bootstrap");
const account_settings_component_1 = require("../dropdowns/account-settings/account-settings.component");
const language_component_1 = require("../dropdowns/language/language.component");
const active_system_component_1 = require("../dropdowns/active-system/active-system.component");
const systems_component_1 = require("../dropdowns/systems/systems.component");
const static_1 = require("@angular/upgrade/static");
let DropdownsModule = class DropdownsModule {
};
DropdownsModule = __decorate([
    core_1.NgModule({
        imports: [
            common_1.CommonModule,
            ng_bootstrap_1.NgbDropdownModule,
            ng_bootstrap_1.NgbModalModule,
        ],
        declarations: [
            language_component_1.NxLanguageDropdown,
            account_settings_component_1.NxAccountSettingsDropdown,
            active_system_component_1.NxActiveSystemDropdown,
            systems_component_1.NxSystemsDropdown
        ],
        entryComponents: [
            language_component_1.NxLanguageDropdown,
            account_settings_component_1.NxAccountSettingsDropdown,
            active_system_component_1.NxActiveSystemDropdown,
            systems_component_1.NxSystemsDropdown
        ],
        exports: [
            language_component_1.NxLanguageDropdown,
            account_settings_component_1.NxAccountSettingsDropdown,
            active_system_component_1.NxActiveSystemDropdown,
            systems_component_1.NxSystemsDropdown
        ]
    })
], DropdownsModule);
exports.DropdownsModule = DropdownsModule;
angular
    .module('cloudApp.directives')
    .directive('nxLanguageSelect', static_1.downgradeComponent({ component: language_component_1.NxLanguageDropdown }))
    .directive('nxAccountSettingsSelect', static_1.downgradeComponent({ component: account_settings_component_1.NxAccountSettingsDropdown }))
    .directive('nxActiveSystem', static_1.downgradeComponent({ component: active_system_component_1.NxActiveSystemDropdown }))
    .directive('nxSystems', static_1.downgradeComponent({ component: systems_component_1.NxSystemsDropdown }));
//# sourceMappingURL=dropdowns.module.js.map