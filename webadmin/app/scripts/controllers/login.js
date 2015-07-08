'use strict';

angular.module('webadminApp')
    .controller('LoginCtrl', function ($scope, mediaserver, ipCookie) {


        // Login digest: http://en.wikipedia.org/wiki/Digest_access_authentication
        // Task: https://hdw.mx/redmine/issues/4812

        $scope.authorized = false;
        $scope.authorizing = false;
        $scope.user = {
            password: "",
            username: ""
        };

        function reload(){
            $scope.authorized = true;
            $scope.authorizing = false;
            setTimeout(function(){
                window.location.reload();
            },20);
            return false;
        }

        $scope.login = function () {
            console.log($scope);
            if ($scope.loginForm.$valid) {

                var lowercaseLogin = $scope.user.username.toLowerCase();
                // Calculate digest
                var realm = ipCookie('realm');
                var nonce = ipCookie('nonce');

                var hash1 = md5(lowercaseLogin + ':' + realm + ':' + $scope.user.password);
                var cnonce = md5("GET:");
                var response = md5(hash1 + ':' + nonce + ':' + cnonce);

                //console.log("hash1 md5(", $scope.user.username + ':' + realm + ':' + $scope.user.password,')=', hash1);
                //console.log("response  md5(", hash1 + ':' + nonce + ':'+ cnonce,')=', response);

                ipCookie('response',response, { path: '/' });
                ipCookie('username',lowercaseLogin, { path: '/' });

                // Check auth again
                $scope.authorizing = true;
                mediaserver.getCurrentUser(true).then(reload).catch(function(error){
                    $scope.authorizing = false;
                    alert("not authorized ");
                });
            }
        };
    });
