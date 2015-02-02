'use strict';

angular.module('webadminApp')
    .controller('LoginCtrl', function ($scope,mediaserver, ipCookie) {


        // Login digest: http://en.wikipedia.org/wiki/Digest_access_authentication
        // Task: https://hdw.mx/redmine/issues/4812

        function reload(){
            window.location.reload();
            return false;
        }

        $scope.login = function () {
            if ($scope.loginForm.$valid) {

                var lowercaseLogin = $scope.user.username.toLowerCase();
                // Calculate digest
                var realm = ipCookie('realm');
                var nonce = ipCookie('nonce');

                var hash1 = md5(lowercaseLogin + ':' + realm + ':' + $scope.user.password);
                var cnonce = md5("GET:");
                var response = md5(hash1 + ':' + nonce + ':' + cnonce);

                ipCookie('response',response, { path: '/' });
                ipCookie('username',lowercaseLogin, { path: '/' });

                // Check auth again
                mediaserver.getCurrentUser(true).then(reload).catch(function(error){
                    alert("not authorized");
                });
            }
        };
    });
