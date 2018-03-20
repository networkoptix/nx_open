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
const forms_1 = require("@angular/forms");
const ng_bootstrap_1 = require("@ng-bootstrap/ng-bootstrap");
const core_2 = require("../../app/core");
const language_component_1 = require("../dropdown/language.component");
const login_component_1 = require("../dialogs/login/login.component");
const process_button_component_1 = require("../components/process-button/process-button.component");
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
            ng_bootstrap_1.NgbModalModule,
            forms_1.FormsModule,
            forms_1.ReactiveFormsModule,
            router_1.RouterModule.forChild(appRoutes)
        ],
        providers: [
            // cloudApiService,
            core_2.QuoteService,
        ],
        declarations: [
            bar_component_1.BarComponent,
            language_component_1.NxLanguageDropdown,
            login_component_1.NxModalLoginComponent,
            process_button_component_1.NxProcessButtonComponent
        ],
        bootstrap: []
    })
], BarModule);
exports.BarModule = BarModule;
//# sourceMappingURL=bar.module.js.map