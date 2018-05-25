(function () {

    'use strict';

    angular
        .module('cloudApp')
        .controller('AccountCtrl', AccountCtrl);

    AccountCtrl.$inject = [ '$scope', 'cloudApi', 'process', '$routeParams', 'account', 'languageService',
        'systemsProvider', 'authorizationCheckService', '$localStorage', 'dialogs' ];

    function AccountCtrl($scope, cloudApi, process, $routeParams, account, languageService,
                         systemsProvider, authorizationCheckService, $localStorage, dialogs) {

        $scope.lang = languageService.lang;

        if ($localStorage && $localStorage.langChanged) {
            $localStorage.langChanged = false;
            dialogs.notify(languageService.lang.account.accountSavedSuccess, 'success', false);
        }

        authorizationCheckService
            .requireLogin()
            .then(function (account) {
                $scope.account = account;
            });

        $scope.accountMode = $routeParams.accountMode;
        $scope.passwordMode = $routeParams.passwordMode;

        $scope.pass = {
            password   : '',
            newPassword: ''
        };

        $scope.save = process.init(function () {
            return cloudApi.accountPost($scope.account)
                .then(function (result) {
                    systemsProvider.forceUpdateSystems();
                    if (languageService.lang.language != $scope.account.language) {
                        $localStorage.langChanged = true;
                        //Need to reload page
                        window.location.reload(true); // reload window to catch new language
                        return false;
                    }
                    return result;
                });
        }, {
            successMessage : languageService.lang.account.accountSavedSuccess,
            errorPrefix    : languageService.lang.errorCodes.cantChangeAccountPrefix,
            logoutForbidden: true
        });

        $scope.changePassword = process.init(function () {
            return cloudApi.changePassword($scope.pass.newPassword, $scope.pass.password);
        }, {
            errorCodes        : {
                notAuthorized   : languageService.lang.errorCodes.oldPasswordMistmatch,
                wrongOldPassword: languageService.lang.errorCodes.oldPasswordMistmatch
            },
            successMessage    : languageService.lang.account.passwordChangedSuccess,
            errorPrefix       : languageService.lang.errorCodes.cantChangePasswordPrefix,
            ignoreUnauthorized: true
        });
    }
})();
