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
const release_component_1 = require("./release/release.component");
const download_history_component_1 = require("./download-history.component");
const appRoutes = [
    { path: 'downloads/history', component: download_history_component_1.DownloadHistoryComponent },
    { path: 'downloads/:build', component: download_history_component_1.DownloadHistoryComponent }
];
let DownloadHistoryModule = class DownloadHistoryModule {
};
DownloadHistoryModule = __decorate([
    core_1.NgModule({
        imports: [
            common_1.CommonModule,
            platform_browser_1.BrowserModule,
            static_1.UpgradeModule,
            ng_bootstrap_1.NgbModule,
            router_1.RouterModule.forChild(appRoutes)
        ],
        providers: [],
        declarations: [
            download_history_component_1.DownloadHistoryComponent,
            release_component_1.ReleaseComponent
        ],
        bootstrap: []
    })
], DownloadHistoryModule);
exports.DownloadHistoryModule = DownloadHistoryModule;
//# sourceMappingURL=download-history.module.js.map