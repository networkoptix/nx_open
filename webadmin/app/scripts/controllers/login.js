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
            console.log($scope);
            if ($scope.loginForm.$valid) {

                // Calculate digest
                var realm = ipCookie('realm');
                var nonce = ipCookie('nonce');

                var hash1 = md5($scope.user.username + ':' + realm + ':' + $scope.user.password);
                var cnonce = md5("GET:");
                var response = md5(hash1 + ':' + nonce + ':' + cnonce);

                //console.log("hash1 md5(", $scope.user.username + ':' + realm + ':' + $scope.user.password,')=', hash1);
                //console.log("response  md5(", hash1 + ':' + nonce + ':'+ cnonce,')=', response);

                ipCookie('response',response, { path: '/' });
                ipCookie('username',$scope.user.username, { path: '/' });

                // Check auth again
                mediaserver.getCurrentUser(true).then(reload).catch(function(error){
                    alert("not authorized");
                });
            }
        };
    });
