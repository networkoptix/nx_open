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
const platform_browser_1 = require("@angular/platform-browser");
const static_1 = require("@angular/upgrade/static");
const router_1 = require("@angular/router");
const ng_bootstrap_1 = require("@ng-bootstrap/ng-bootstrap");
const angular_uuid2_1 = require("../../app/scripts/services/angular-uuid2");
const core_2 = require("../../app/core");
const language_1 = require("../scripts/services/language");
const language_component_1 = require("../dropdown/language.component");
const bar_component_1 = require("./bar.component");
const appRoutes = [
    { path: 'bar', component: bar_component_1.BarComponent }
];
let BarModule = class BarModule {
};
BarModule = __decorate([
    core_1.NgModule({
        imports: [
            common_1.CommonModule,
            platform_browser_1.BrowserModule,
            static_1.UpgradeModule,
            ng_bootstrap_1.NgbDropdownModule,
            router_1.RouterModule.forChild(appRoutes)
        ],
        providers: [
            core_2.QuoteService,
            angular_uuid2_1.uuid2Service,
            language_1.languageService
        ],
        declarations: [bar_component_1.BarComponent, language_component_1.NxLanguageDropdown],
        bootstrap: []
    })
], BarModule);
exports.BarModule = BarModule;
//# sourceMappingURL=bar.module.js.map