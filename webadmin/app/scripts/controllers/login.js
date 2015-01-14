'use strict';

angular.module('webadminApp')
    .controller('LoginCtrl', function ($scope,mediaserver, $cookies) {


        // Login digest: http://en.wikipedia.org/wiki/Digest_access_authentication
        // Task: https://hdw.mx/redmine/issues/4812

        function reload(){
            window.location.reload();
            return false;
        }

        function createCookie(name,value,days) {
            if (days) {
                var date = new Date();
                date.setTime(date.getTime()+(days*24*60*60*1000));
                var expires = "; expires="+date.toGMTString();
            }
            else var expires = "";
            document.cookie = name+"="+value+expires+"; path=/";
        }

        function readCookie(name) {
            console.log("cookies",document.cookie);
            var nameEQ = name + "=";
            var ca = document.cookie.split(';');
            for(var i=0;i < ca.length;i++) {
                var c = ca[i];
                while (c.charAt(0)==' ') c = c.substring(1,c.length);
                if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length,c.length);
            }
            return null;
        }

        function eraseCookie(name) {
            createCookie(name,"",-1);
        }


        $scope.login = function () {
            if ($scope.loginForm.$valid) {

                // Calculate digest
                var realm = readCookie('realm');
                var nonce = readCookie('nonce');

                var hash1 = md5($scope.user.username + ':' + realm + ':' + $scope.user.password);
                var cnonce = md5("GET:");
                var response = md5(hash1 + ':' + nonce + ':' + cnonce);

                console.log("hash1 md5(", $scope.user.username + ':' + realm + ':' + $scope.user.password,')=', hash1);
                console.log("response  md5(", hash1 + ':' + nonce + ':'+ cnonce,')=', response);

                createCookie('username',$scope.user.username);
                createCookie('response',response);

                // Check auth again
                mediaserver.getCurrentUser(true).then(reload).catch(function(error){
                    alert("not authorized");
                });
            }
        };
    });
