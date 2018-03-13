"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const cloud_api_1 = require("../app/scripts/services/cloud_api");
const language_1 = require("../app/scripts/services/language");
const angular_uuid2_1 = require("../app/scripts/services/angular-uuid2");
function cloudApiFactory(i) {
    return i.get('cloudApi');
}
exports.cloudApiFactory = cloudApiFactory;
exports.cloudApiServiceProvider = {
    provide: cloud_api_1.cloudApiService,
    useFactory: cloudApiFactory,
    deps: ['$injector']
};
function languageFactory(i) {
    return i.get('languageService');
}
exports.languageFactory = languageFactory;
exports.languageServiceProvider = {
    provide: language_1.languageService,
    useFactory: languageFactory,
    deps: ['$injector']
};
function uuid2Factory(i) {
    return i.get('uuid2');
}
exports.uuid2Factory = uuid2Factory;
exports.uuid2ServiceProvider = {
    provide: angular_uuid2_1.uuid2Service,
    useFactory: uuid2Factory,
    deps: ['$injector']
};
//# sourceMappingURL=ajs-upgraded-providers.js.map