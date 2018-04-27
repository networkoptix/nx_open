import * as angular from 'angular';

(function () {

    'use strict';

    angular
        .module('cloudApp.services')
        .provider('languageService', [

            function () {

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

                // config phaze accessible functions **************
                this.setLanguage = function (language) {
                    lang = language;
                    // lang.language = lang.language.replace('-', '_');
                };

                this.setCommonLanguage = function (language) {
                    lang.common = language;
                };
                // ************************************************

                this.$get = function () {
                    return {
                        // runtime accessible
                        lang: lang
                    }
                }

            }]);
})();
