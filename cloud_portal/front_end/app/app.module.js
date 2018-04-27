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
const animations_1 = require("@angular/platform-browser/animations");
const static_1 = require("@angular/upgrade/static");
const router_1 = require("@angular/router");
const http_1 = require("@angular/common/http");
const forms_1 = require("@angular/forms");
const ng_bootstrap_1 = require("@ng-bootstrap/ng-bootstrap");
const ng_bootstrap_2 = require("@ng-bootstrap/ng-bootstrap");
const ngx_order_pipe_1 = require("ngx-order-pipe");
const ngx_device_detector_1 = require("ngx-device-detector");
const core_2 = require("@ngx-translate/core");
const http_loader_1 = require("@ngx-translate/http-loader");
const ngx_cookie_service_1 = require("ngx-cookie-service");
const index_1 = require("./src/core/index");
const ajs_upgraded_providers_1 = require("./ajs-upgraded-providers");
const ajs_upgraded_providers_2 = require("./ajs-upgraded-providers");
const ajs_upgraded_providers_3 = require("./ajs-upgraded-providers");
const ajs_upgraded_providers_4 = require("./ajs-upgraded-providers");
const ajs_upgraded_providers_5 = require("./ajs-upgraded-providers");
const ajs_upgraded_providers_6 = require("./ajs-upgraded-providers");
const ajs_upgraded_providers_7 = require("./ajs-upgraded-providers");
const ajs_upgraded_providers_8 = require("./ajs-upgraded-providers");
const ajs_upgraded_providers_9 = require("./ajs-upgraded-providers");
const app_component_1 = require("./app.component");
const bar_module_1 = require("./src/bar/bar.module");
const download_module_1 = require("./src/download/download.module");
const download_history_module_1 = require("./src/download-history/download-history.module");
const dropdowns_module_1 = require("./src/dropdowns/dropdowns.module");
const login_component_1 = require("./src/dialogs/login/login.component");
const process_button_component_1 = require("./src/components/process-button/process-button.component");
const dialogs_service_1 = require("./src/dialogs/dialogs.service");
const general_component_1 = require("./src/dialogs/general/general.component");
const disconnect_component_1 = require("./src/dialogs/disconnect/disconnect.component");
const rename_component_1 = require("./src/dialogs/rename/rename.component");
const share_component_1 = require("./src/dialogs/share/share.component");
const merge_component_1 = require("./src/dialogs/merge/merge.component");
// AoT requires an exported function for factories
function createTranslateLoader(http) {
    return new http_loader_1.TranslateHttpLoader(http, 'static/lang_', '/language_i18n.json');
}
exports.createTranslateLoader = createTranslateLoader;
class HybridUrlHandlingStrategy {
    shouldProcessUrl(url) {
        return url.toString().startsWith('/bar') ||
            url.toString().startsWith('/download') ||
            url.toString().startsWith('/downloads');
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
            animations_1.BrowserAnimationsModule,
            static_1.UpgradeModule,
            http_1.HttpClientModule,
            forms_1.FormsModule,
            ngx_order_pipe_1.OrderModule,
            index_1.CoreModule,
            bar_module_1.BarModule,
            download_module_1.DownloadModule,
            download_history_module_1.DownloadHistoryModule,
            ajs_upgraded_providers_1.cloudApiServiceModule,
            ajs_upgraded_providers_6.uuid2ServiceModule,
            ajs_upgraded_providers_3.languageServiceModule,
            ajs_upgraded_providers_4.accountServiceModule,
            ajs_upgraded_providers_5.processServiceModule,
            ajs_upgraded_providers_2.systemsModule,
            ajs_upgraded_providers_7.ngToastModule,
            ajs_upgraded_providers_8.configServiceModule,
            ajs_upgraded_providers_9.authorizationCheckServiceModule,
            dropdowns_module_1.DropdownsModule,
            core_2.TranslateModule.forRoot({
                loader: {
                    provide: core_2.TranslateLoader,
                    useFactory: (createTranslateLoader),
                    deps: [http_1.HttpClient]
                }
            }),
            ngx_device_detector_1.DeviceDetectorModule.forRoot(),
            ng_bootstrap_1.NgbModule.forRoot(),
            router_1.RouterModule.forRoot([], { initialNavigation: false })
        ],
        entryComponents: [
            process_button_component_1.NxProcessButtonComponent,
            login_component_1.LoginModalContent, login_component_1.NxModalLoginComponent,
            general_component_1.GeneralModalContent, general_component_1.NxModalGeneralComponent,
            disconnect_component_1.DisconnectModalContent, disconnect_component_1.NxModalDisconnectComponent,
            rename_component_1.RenameModalContent, rename_component_1.NxModalRenameComponent,
            share_component_1.ShareModalContent, share_component_1.NxModalShareComponent,
            merge_component_1.MergeModalContent, merge_component_1.NxModalMergeComponent
        ],
        providers: [
            ng_bootstrap_2.NgbModal,
            common_1.Location,
            ngx_cookie_service_1.CookieService,
            { provide: common_1.LocationStrategy, useClass: common_1.PathLocationStrategy },
            { provide: router_1.UrlHandlingStrategy, useClass: HybridUrlHandlingStrategy },
            // {provide: '$scope', useFactory: i => i.get('$rootScope'), deps: ['$injector']},
            // {provide: '$rootScope', useFactory: i => i.get('$rootScope'), deps: ['$injector']},
            login_component_1.NxModalLoginComponent,
            general_component_1.NxModalGeneralComponent,
            disconnect_component_1.NxModalDisconnectComponent,
            rename_component_1.NxModalRenameComponent,
            share_component_1.NxModalShareComponent,
            merge_component_1.NxModalMergeComponent,
            process_button_component_1.NxProcessButtonComponent,
            dialogs_service_1.nxDialogsService
        ],
        declarations: [
            app_component_1.AppComponent,
            login_component_1.LoginModalContent, login_component_1.NxModalLoginComponent,
            general_component_1.GeneralModalContent, general_component_1.NxModalGeneralComponent,
            disconnect_component_1.DisconnectModalContent, disconnect_component_1.NxModalDisconnectComponent,
            rename_component_1.RenameModalContent, rename_component_1.NxModalRenameComponent,
            share_component_1.ShareModalContent, share_component_1.NxModalShareComponent,
            merge_component_1.MergeModalContent, merge_component_1.NxModalMergeComponent,
            process_button_component_1.NxProcessButtonComponent,
        ],
        bootstrap: [app_component_1.AppComponent]
    })
], AppModule);
exports.AppModule = AppModule;
angular
    .module('cloudApp.directives')
    .directive('nxModalLogin', static_1.downgradeComponent({ component: login_component_1.NxModalLoginComponent }));
angular
    .module('cloudApp.services')
    .service('nxDialogsService', static_1.downgradeInjectable(dialogs_service_1.nxDialogsService));
//# sourceMappingURL=app.module.js.map