(function () {

    'use strict';

    angular
        .module('cloudApp.services')
        .provider('languageService',

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
                    lang.language = lang.language.replace('-', '_');
                };

                this.setCommonLanguage = function (language) {
                    lang.common = language;
                };

                this.$get = function () {
                    return {
                        lang: lang
                    };
                };

            });
})();
