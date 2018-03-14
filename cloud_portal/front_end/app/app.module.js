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
const http_1 = require("@angular/common/http");
const ng_bootstrap_1 = require("@ng-bootstrap/ng-bootstrap");
const index_1 = require("./core/index");
const ajs_upgraded_providers_1 = require("./ajs-upgraded-providers");
const ajs_upgraded_providers_2 = require("./ajs-upgraded-providers");
const ajs_upgraded_providers_3 = require("./ajs-upgraded-providers");
const app_component_1 = require("./app.component");
const bar_module_1 = require("./bar/bar.module");
const language_component_1 = require("./dropdown/language.component");
class HybridUrlHandlingStrategy {
    // use only process the `/bar` url
    shouldProcessUrl(url) {
        return url.toString().startsWith('/bar');
    }
    extract(url) {
        return url;
    }
    merge(url, whole) {
        return url;
    }
}
let AppModule = class AppModule {
    ngDoBootstrap() {
    }
};
AppModule = __decorate([
    core_1.NgModule({
        imports: [
            common_1.CommonModule,
            platform_browser_1.BrowserModule,
            static_1.UpgradeModule,
            http_1.HttpClientModule,
            index_1.CoreModule,
            bar_module_1.BarModule,
            ajs_upgraded_providers_3.uuid2ServiceModule,
            ajs_upgraded_providers_2.languageServiceModule,
            ng_bootstrap_1.NgbModule.forRoot(),
            router_1.RouterModule.forRoot([], { initialNavigation: false })
        ],
        entryComponents: [
            language_component_1.NxLanguageDropdown
        ],
        providers: [
            { provide: common_1.LocationStrategy, useClass: common_1.PathLocationStrategy },
            { provide: router_1.UrlHandlingStrategy, useClass: HybridUrlHandlingStrategy },
            ajs_upgraded_providers_1.cloudApiServiceProvider
        ],
        declarations: [app_component_1.AppComponent],
        bootstrap: [app_component_1.AppComponent]
    })
], AppModule);
exports.AppModule = AppModule;
angular
    .module('cloudApp.directives')
    .directive('nxLanguageSelect', static_1.downgradeComponent({ component: language_component_1.NxLanguageDropdown }));
//# sourceMappingURL=app.module.js.map