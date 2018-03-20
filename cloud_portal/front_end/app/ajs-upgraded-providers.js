"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", { value: true });
const core_1 = require("@angular/core");
// import { cloudApiService } from '../app/scripts/services/cloud_api';
// import { languageService } from '../app/scripts/services/language';
// import { uuid2Service } from '../app/scripts/services/angular-uuid2';
function cloudApiFactory(i) {
    return i.get('cloudApi');
}
exports.cloudApiFactory = cloudApiFactory;
exports.cloudApiServiceProvider = {
    provide: 'cloudApiService',
    useFactory: cloudApiFactory,
    deps: ['$injector']
};
function languageFactory(i) {
    return i.get('languageService');
}
exports.languageFactory = languageFactory;
exports.languageServiceProvider = {
    provide: 'languageService',
    useFactory: languageFactory,
    deps: ['$injector']
};
function accountFactory(i) {
    return i.get('accountService');
}
exports.accountFactory = accountFactory;
exports.accountServiceProvider = {
    provide: 'accountService',
    useFactory: accountFactory,
    deps: ['$injector']
};
function processFactory(i) {
    return i.get('processService');
}
exports.processFactory = processFactory;
exports.processServiceProvider = {
    provide: 'processService',
    useFactory: processFactory,
    deps: ['$injector']
};
function uuid2Factory(i) {
    return i.get('uuid2');
}
exports.uuid2Factory = uuid2Factory;
exports.uuid2ServiceProvider = {
    provide: 'uuid2Service',
    useFactory: uuid2Factory,
    deps: ['$injector']
};
let cloudApiServiceModule = class cloudApiServiceModule {
};
cloudApiServiceModule = __decorate([
    core_1.NgModule({
        providers: [
            exports.cloudApiServiceProvider
        ]
    })
], cloudApiServiceModule);
exports.cloudApiServiceModule = cloudApiServiceModule;
let uuid2ServiceModule = class uuid2ServiceModule {
};
uuid2ServiceModule = __decorate([
    core_1.NgModule({
        providers: [
            exports.uuid2ServiceProvider
        ]
    })
], uuid2ServiceModule);
exports.uuid2ServiceModule = uuid2ServiceModule;
let languageServiceModule = class languageServiceModule {
};
languageServiceModule = __decorate([
    core_1.NgModule({
        providers: [
            exports.languageServiceProvider
        ]
    })
], languageServiceModule);
exports.languageServiceModule = languageServiceModule;
let accountServiceModule = class accountServiceModule {
};
accountServiceModule = __decorate([
    core_1.NgModule({
        providers: [
            exports.accountServiceProvider
        ]
    })
], accountServiceModule);
exports.accountServiceModule = accountServiceModule;
let processServiceModule = class processServiceModule {
};
processServiceModule = __decorate([
    core_1.NgModule({
        providers: [
            exports.processServiceProvider
        ]
    })
], processServiceModule);
exports.processServiceModule = processServiceModule;
//# sourceMappingURL=ajs-upgraded-providers.js.map