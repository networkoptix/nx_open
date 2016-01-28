'use strict';

angular.module('cloudApp')
    .controller('DebugCtrl', function ($scope, cloudApi, account) {

        account.requireLogin();

        $scope.user_email = "ebalashov@networkoptix.com";
        $scope.type = "activate_account";
        $scope.message = JSON.stringify({code:"test_code"});


        $scope.testNotification = function(){
            var message = $scope.message;
            try {
                message = JSON.parse(message);
            }catch(a){
                console.warn ("message is not json", message);
            }
            cloudApi.notification_send($scope.user_email,$scope.type,message).
                then(
                function(a){
                    console.log(a);
                    alert ("Success " + + a.data.errorText);
                },
                function(a){
                    console.log(a);
                    alert ("Error " + a.data.errorText);
                }
            )
        }


    });