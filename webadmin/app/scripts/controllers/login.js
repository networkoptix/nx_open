'use strict';

angular.module('webadminApp')
    .controller('LoginCtrl', ['$scope', 'mediaserver', '$sessionStorage', 'dialogs', 'nativeClient', '$timeout',
    function ($scope, mediaserver, $sessionStorage, dialogs, nativeClient, $timeout) {

        // A hack that allows the user to change the url and remove the login dialog.
        $scope.$on('$locationChangeSuccess', function() {
            window.location.reload();
        });

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
            $timeout(function(){
                window.location.reload();
            }, 20);
            return false;
        }
    
        function touchForm(form) {
            for (let ctrl in form) {
                if (ctrl.indexOf('$') === -1) {
                    form[ctrl].$setTouched(true);
                    form[ctrl].$setDirty(true);
                }
            }
        }

        $scope.login = function (form) {
            $timeout(function() {
                if ($scope.loginForm.$valid && !$scope.authorized && !$scope.authorizing) {
                    $scope.authorizing = true;
                    touchForm(form);
                    var login = $scope.user.username.toLowerCase();
                    var password = $scope.user.password;

                    mediaserver
                        .login(login, password)
                        .then(reload, function (error) {
                            var alertMessage = L.login.authLockout;
                            var authResult = error.authResult !== '' &&
                                ['Auth_WrongLogin', 'Auth_WrongPassword'].some(function(code){
                                    return code === error.authResult;
                                });
                            if ( authResult || error.errorString.toLowerCase().indexOf('wrong') > -1) {
                                alertMessage = L.login.incorrectPassword;
                            }
                            return dialogs.alert(alertMessage);
                        }).then(function () {
                            nativeClient.updateCredentials(login, password, false, false);
                        }).finally(function () {
                            $scope.authorizing = false;
                        });
                }
            }, 50);
        };
    }]);
