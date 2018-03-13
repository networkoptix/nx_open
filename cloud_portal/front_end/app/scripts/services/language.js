"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", { value: true });
const angular = require("angular");
(function () {
    'use strict';
    angular
        .module('cloudApp.services')
        .provider('languageService', [
        function () {
            var lang = {
                language: '',
                pageTitles: {
                    registerSuccess: '',
                    register: '',
                    account: '',
                    system: '',
                    systemShare: '',
                    systems: '',
                    view: '',
                    changePassword: '',
                    activate: '',
                    activateCode: '',
                    activateSuccess: '',
                    restorePassword: '',
                    restorePasswordCode: '',
                    restorePasswordSuccess: '',
                    debug: '',
                    login: '',
                    download: '',
                    downloadPlatform: '',
                    pageNotFound: ''
                },
                common: {}
            };
            this.setLanguage = function (language) {
                lang = language;
            };
            this.setCommonLanguage = function (language) {
                lang.common = language;
            };
            this.$get = function () {
                return {
                    lang: lang
                };
            };
        }
    ]);
})();
const core_1 = require("@angular/core");
let languageService = class languageService {
    get() {
        let lang = {
            language: '',
            pageTitles: {
                registerSuccess: '',
                register: '',
                account: '',
                system: '',
                systemShare: '',
                systems: '',
                view: '',
                changePassword: '',
                activate: '',
                activateCode: '',
                activateSuccess: '',
                restorePassword: '',
                restorePasswordCode: '',
                restorePasswordSuccess: '',
                debug: '',
                login: '',
                download: '',
                downloadPlatform: '',
                pageNotFound: ''
            },
            common: {}
        };
        return {
            setLanguage: function (language) {
                lang = language;
            },
            setCommonLanguage: function (language) {
                lang.common = language;
            },
            $get: function () {
                return {
                    lang: lang
                };
            },
            getLanguage: function () {
                return lang;
            }
            // getLanguage: () => lang
        };
    }
};
languageService = __decorate([
    core_1.Injectable()
], languageService);
exports.languageService = languageService;
//# sourceMappingURL=language.js.map