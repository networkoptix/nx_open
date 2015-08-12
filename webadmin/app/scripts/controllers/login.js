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

                /*
                console.log("debug auth - realm:",realm);
                console.log("debug auth - nonce:",nonce);
                console.log("debug auth - login:",lowercaseLogin);
                console.log("debug auth - password:",$scope.user.password);
                console.log("debug auth - digest = md5(login:realm:password):",digest);
                console.log("debug auth - method:","GET:");
                console.log("debug auth - md5(method):",method);
                console.log("debug auth -  auth_digest = md5(digest:nonce:md5(method)):",auth_digest);
                console.log("debug auth -  auth = base64(login:nonce:auth_digest):",auth);
                */

                ipCookie('auth',auth, { path: '/' });

                var rtspmethod = md5("PLAY:");
                var rtsp_digest = md5(digest + ':' + nonce + ':' + rtspmethod);
                var auth_rtsp = Base64.encode(lowercaseLogin + ':' + nonce + ':' + rtsp_digest);
                ipCookie('auth_rtsp',auth_rtsp, { path: '/' });

                //Old cookies:  // TODO: REMOVE THIS SECTION
                //ipCookie('response',auth_digest, { path: '/' });
                //ipCookie('username',lowercaseLogin, { path: '/' });

                // Check auth again
                $scope.authorizing = true;
                mediaserver.getCurrentUser(true).then(reload).catch(function(error){
                    $scope.authorizing = false;
                    alert("Login or password is incorrect");
                });
            }
        };
    });
