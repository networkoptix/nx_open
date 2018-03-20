(function () {
    'use strict';
    angular
        .module('cloudApp.directives')
        .directive('nxLogin', ['$location', '$routeParams', 'CONFIG', 'dialogs', 'account', 'process', 'languageService',
        function ($location, $routeParams, CONFIG, dialogs, account, process, languageService) {
            return {
                restrict: 'E',
                transclude: 'element',
                template: '<button (click)="open()">test</button>',
                controller: function () {
                    var login = this, lang = languageService.lang, dialogSettings = dialogs.getSettings(login);
                    login.auth = {
                        email: '',
                        password: '',
                        remember: true
                    };
                    login.open = dialogs.op;
                    login.close = function () {
                        account.setEmail(login.auth.email);
                        dialogs.closeMe(login);
                    };
                    login.$on('$destroy', function () {
                        dialogs.dismissNotifications();
                    });
                    login.login = process.init(function () {
                        return account.login(login.auth.email, login.auth.password, login.auth.remember);
                    }, {
                        ignoreUnauthorized: true,
                        errorCodes: {
                            accountNotActivated: function () {
                                $location.path('/activate');
                                return false;
                            },
                            notFound: lang.errorCodes.emailNotFound,
                            portalError: lang.errorCodes.brokenAccount
                        }
                    }).then(function () {
                        if (dialogSettings.params.redirect) {
                            $location.path($routeParams.next ? $routeParams.next : CONFIG.redirectAuthorised);
                        }
                        setTimeout(function () {
                            document.location.reload();
                        });
                    });
                }
            };
        }]);
})();
//# sourceMappingURL=login.controller.js.map