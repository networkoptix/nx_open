'use strict';

angular.module('cloudApp')
    .controller('AccountCtrl', ['$scope', 'cloudApi', 'process', '$routeParams', 'account', '$timeout', 'systemsProvider',
    function ($scope, cloudApi, process, $routeParams, account, $timeout, systemsProvider) {

        account.requireLogin().then(function(account){
            $scope.account = account;
        });

        $scope.accountMode = $routeParams.accountMode;
        $scope.passwordMode =  $routeParams.passwordMode;

        $scope.pass = {
            password: '',
            newPassword: ''
        };

        $scope.save = process.init(function() {
            return cloudApi.accountPost($scope.account).then(function(result){
                systemsProvider.forceUpdateSystems();
                if(L.language != $scope.account.language){
                    //Need to reload page
                    window.location.reload(true); // reload window to catch new language
                    return false;
                }
                return result;
            });
        },{
            successMessage:L.account.accountSavedSuccess,
            errorPrefix: L.errorCodes.cantChangeAccountPrefix,
            logoutForbidden: true
        });

        $scope.changePassword = process.init(function() {
            return cloudApi.changePassword($scope.pass.newPassword,$scope.pass.password);
        },{
            errorCodes:{
                notAuthorized: L.errorCodes.oldPasswordMistmatch,
                wrongOldPassword: L.errorCodes.oldPasswordMistmatch
            },
            successMessage:L.account.passwordChangedSuccess,
            errorPrefix:L.errorCodes.cantChangePasswordPrefix,
            ignoreUnauthorized: true
        });
    }]);