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

                var digest = md5(lowercaseLogin + ':' + realm + ':' + $scope.user.password);
                var method = md5("GET:");
                var auth_digest = md5(digest + ':' + nonce + ':' + method);
                var auth = Base64.encode(lowercaseLogin + ':' + nonce + ':' + auth_digest);
                ipCookie('auth',auth, { path: '/' });

                var rtspmethod = md5("PLAY:");
                var rtsp_digest = md5(digest + ':' + nonce + ':' + rtspmethod);
                var auth_rtsp = Base64.encode(lowercaseLogin + ':' + nonce + ':' + rtsp_digest);
                ipCookie('auth_rtsp',auth_rtsp, { path: '/' });


                //Old cookies:
                ipCookie('response',auth_digest, { path: '/' }); // TODO: REMOVE
                ipCookie('username',lowercaseLogin, { path: '/' }); // TODO: REMOVE
                //var authKey = Base64.encode(lowercaseLogin + ':0:' + digest); // TODO: REMOVE
                //ipCookie('authKey',authKey, { path: '/' }); // TODO: REMOVE

                // Check auth again
                $scope.authorizing = true;
                mediaserver.getCurrentUser(true).then(reload).catch(function(error){
                    $scope.authorizing = false;
                    alert("Login or password is incorrect");
                });
            }
        };
    });
