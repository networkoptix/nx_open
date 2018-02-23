(function () {

    'use strict';

    angular
        .module('cloudApp.services')
        .provider('languageService',

            function () {

                var lang = {
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
