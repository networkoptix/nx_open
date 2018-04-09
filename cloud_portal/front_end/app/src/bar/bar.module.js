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
const forms_1 = require("@angular/forms");
// import { NgbModalModule, NgbDropdownModule } from '@ng-bootstrap/ng-bootstrap';
const index_1 = require("../core/index");
const dropdowns_module_1 = require("../../src/dropdowns/dropdowns.module");
const bar_component_1 = require("./bar.component");
// import { NxActiveSystemDropdown }    from "../dropdowns/active-system/active-system.component";
// import { NxSystemsDropdown }         from "../dropdowns/systems/systems.component";
// const appRoutes: Routes = [
//     {path: 'bar', component: BarComponent}
// ];
let BarModule = class BarModule {
};
BarModule = __decorate([
    core_1.NgModule({
        imports: [
            common_1.CommonModule,
            platform_browser_1.BrowserModule,
            static_1.UpgradeModule,
            // NgbDropdownModule,
            // NgbModalModule,
            forms_1.FormsModule,
            forms_1.ReactiveFormsModule,
            dropdowns_module_1.DropdownsModule,
        ],
        providers: [
            index_1.QuoteService,
            dropdowns_module_1.DropdownsModule
        ],
        declarations: [
            bar_component_1.BarComponent,
        ],
        bootstrap: []
    })
], BarModule);
exports.BarModule = BarModule;
//# sourceMappingURL=bar.module.js.map