'use strict';

angular.module('cloudApp')
    .controller('DebugCtrl', function ($scope, cloudApi, account, process, $q, $timeout,dialogs) {

        account.requireLogin();

        $scope.user_email = "ebalashov@networkoptix.com";
        $scope.type = "activate_account";
        $scope.message = JSON.stringify({code:"test_code"}, null, "\t");

        $scope.formatJSON = function(data){
            return  JSON.stringify(data, null, "\t");
        };

        $scope.debugProcess = {
            success: true,
            process: process.init(function () {
                var deferred = $q.defer();
                $timeout(function () {
                    if ($scope.debugProcess.success) {
                        deferred.resolve({
                            data:{
                                resultCode: L.errorCodes.ok
                            }
                        });
                    } else {
                        deferred.reject(false);
                    }
                }, 2000);
                return deferred.promise;
            },{
                successMessage:'Success!',
                errorPrefix:'Fail!'
            })
        };
        var notifyCounter = 0;
        $scope.notify = function(){
            var states = ['warning','info','success','danger'];
            var type = states[Math.floor(Math.random() * states.length)];
            var hold = Math.random() > 0.9;
            dialogs.notify((notifyCounter++) + ":" + type + ": " + hold, type, hold);
        };
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