'use strict';
/*global md5, Base64 */
angular.module('webadminApp')
    .controller('LoginCtrl', function ($scope, mediaserver, ipCookie,$sessionStorage) {


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

                var lowercaseLogin = $scope.user.username.toLowerCase();
                // Calculate digest
                var realm = ipCookie('realm');
                var nonce = ipCookie('nonce');

                var digest = md5(lowercaseLogin + ':' + realm + ':' + $scope.user.password);
                var method = md5('GET:');
                var authDigest = md5(digest + ':' + nonce + ':' + method);
                var auth = Base64.encode(lowercaseLogin + ':' + nonce + ':' + authDigest);

                /*
                console.log('debug auth - realm:',realm);
                console.log('debug auth - nonce:',nonce);
                console.log('debug auth - login:',lowercaseLogin);
                console.log('debug auth - password:',$scope.user.password);
                console.log('debug auth - digest = md5(login:realm:password):',digest);
                console.log('debug auth - method:','GET:');
                console.log('debug auth - md5(method):',method);
                console.log('debug auth -  authDigest = md5(digest:nonce:md5(method)):',authDigest);
                console.log('debug auth -  auth = base64(login:nonce:authDigest):',auth);
                */

                ipCookie('auth',auth, { path: '/' });

                var rtspmethod = md5('PLAY:');
                var rtspDigest = md5(digest + ':' + nonce + ':' + rtspmethod);
                var authRtsp = Base64.encode(lowercaseLogin + ':' + nonce + ':' + rtspDigest);
                ipCookie('auth_rtsp',authRtsp, { path: '/' });

                //Old cookies:  // TODO: REMOVE THIS SECTION
                //ipCookie('response',auth_digest, { path: '/' });
                //ipCookie('username',lowercaseLogin, { path: '/' });

                // Check auth again
                $scope.authorizing = true;
                mediaserver.getCurrentUser(true).then(reload).catch(function(/*error*/){
                    $scope.authorizing = false;
                    alert('Login or password is incorrect');
                });
            }
        };
    });
