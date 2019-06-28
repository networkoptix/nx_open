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
    
        function touchForm(form) {
            for (let ctrl in form) {
                if (ctrl.indexOf('$') === -1) {
                    form[ctrl].$setTouched(true);
                    form[ctrl].$setDirty(true);
                }
            }
        }

        $scope.login = function (form) {
            touchForm(form);
            
            if ($scope.loginForm.$valid) {
                var login = $scope.user.username.toLowerCase();
                var password = $scope.user.password;
                $scope.authorizing = true;
                
                mediaserver
                    .login(login, password)
                    .then(reload, function(error) {
                        $scope.authorizing = false;
                        if (error.authResult !== '') {
                            switch (error.authResult) {
                                case 'Auth_WrongLogin':
                                case 'Auth_WrongPassword':
                                    dialogs.alert(L.login.incorrectPassword);
                                    break;
                                case 'Auth_LockedOut':
                                default:
                                    dialogs.alert(L.login.authLockout);
                            }
                        } else {
                            if(error.errorString.toLowerCase().indexOf('wrong') > -1) {
                                dialogs.alert(L.login.incorrectPassword);
                            } else {
                                dialogs.alert(L.login.authLockout);
                            }
                        }
                    }).then(function(){
                        nativeClient.updateCredentials(login, password, false, false);
                    });
            }
        };
    }]);
