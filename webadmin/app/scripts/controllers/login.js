'use strict';

angular.module('webadminApp')
    .controller('LoginCtrl', ['$scope', 'mediaserver', '$sessionStorage', 'dialogs', 'nativeClient',
    function ($scope, mediaserver, $sessionStorage, dialogs, nativeClient) {


        // Login digest: http://en.wikipedia.org/wiki/Digest_access_authentication
        // Task: https://hdw.mx/redmine/issues/4812

        $scope.authorized = false;
        $scope.authorizing = false;
        if(!$scope.user){
            $scope.user = {
                username:'',
                password:''
            };
        }
        $scope.session = $sessionStorage;
        function reload(){
            $scope.authorized = true;
            $scope.authorizing = false;
            $scope.session.serverInfoAlertHidden = false;
            setTimeout(function(){
                window.location.reload();
            },20);
            return false;
        }

        $scope.login = function () {
            if ($scope.loginForm.$valid) {
                var login = $scope.user.username.toLowerCase();
                var password = $scope.user.password;
                $scope.authorizing = true;
                mediaserver.login(login,password).then(reload,function(/*error*/){
                    $scope.authorizing = false;
                    dialogs.alert(L.login.incorrectPassword);
                }).then(function(){
                    nativeClient.updateCredentials(login,password,false,false);
                });
            }
        };
    }]);
