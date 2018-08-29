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
