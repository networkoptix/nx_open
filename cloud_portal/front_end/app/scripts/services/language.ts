import * as angular from 'angular';

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

                    }]);
})();

import { Injectable } from '@angular/core';

@Injectable()
export class languageService {
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
            setLanguage: function (language: any) {
                lang = language;
            },

            setCommonLanguage: function (language: any) {
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
        }
    }
}
