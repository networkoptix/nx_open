'use strict';

angular.module('webadminApp')
    .controller('LoginCtrl', function ($scope, mediaserver, ipCookie) {


        // Login digest: http://en.wikipedia.org/wiki/Digest_access_authentication
        // Task: https://hdw.mx/redmine/issues/4812

        $scope.authorized = false;
        $scope.authorizing = false;
        if(!$scope.user){
            $scope.user = {
                username:"",
                password:""
            }
        }

        function reload(){
            $scope.authorized = true;
            $scope.authorizing = false;
            setTimeout(function(){
                console.log("reload on login");
                window.location.reload();
            },20);
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
                var rtspcnonce = md5("PLAY:");
                var response = md5(hash1 + ':' + nonce + ':' + cnonce);

                var rtspAuth = md5(hash1 + ':' + nonce + ':' + rtspcnonce);
                ipCookie('rtspAuth',rtspAuth, { path: '/' });

                ipCookie('response',response, { path: '/' });
                ipCookie('username',lowercaseLogin, { path: '/' });

                var auth = Base64.encode(lowercaseLogin + ':0:' + hash1)
                ipCookie('auth',auth, { path: '/' });



                // Check auth again
                $scope.authorizing = true;
                mediaserver.getCurrentUser(true).then(reload).catch(function(error){
                    $scope.authorizing = false;
                    alert("not authorized ");
                });
            }
        };
    });
