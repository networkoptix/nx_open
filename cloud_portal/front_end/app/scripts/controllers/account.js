'use strict';

angular.module('cloudApp')
    .controller('AccountCtrl', ['$scope', 'cloudApi', 'process', '$routeParams', 'account', '$timeout',
    function ($scope, cloudApi, process, $routeParams, account, $timeout) {

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
                if(L.language != account.language){
                    //Need to reload page
                    $timeout(function(){
                        window.location.reload(true); // reload window to catch new language
                    },Config.alertTimeout);
                }
                return result;
            });
        },{
            successMessage:'Your account was successfully saved.',
            errorPrefix:'Couldn\'t save your data:',
            logoutForbidden: true
        });

        $scope.changePassword = process.init(function() {
            return cloudApi.changePassword($scope.pass.newPassword,$scope.pass.password);
        },{
            errorCodes:{
                notAuthorized: L.errorCodes.oldPasswordMistmatch
            },
            successMessage:'Your password was successfully changed.',
            errorPrefix:'Couldn\'t change your password:',
            ignoreUnauthorized: true
        });
    }]);