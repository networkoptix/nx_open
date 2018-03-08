"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const platform_browser_dynamic_1 = require("@angular/platform-browser-dynamic");
const static_1 = require("@angular/upgrade/static");
const app_module_1 = require("./app.module");
const upgrade_1 = require("@angular/router/upgrade");
// import { Router } from '@angular/router';
platform_browser_dynamic_1.platformBrowserDynamic().bootstrapModule(app_module_1.AppModule).then(platformRef => {
    const upgrade = platformRef.injector.get(static_1.UpgradeModule);
    upgrade.bootstrap(document.documentElement, ['cloudApp']);
    // platformRef.injector.get(Router).initialNavigation();
    upgrade_1.setUpLocationSync(upgrade);
});
//# sourceMappingURL=main.js.map