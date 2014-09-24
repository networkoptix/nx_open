'use strict';

angular.module('webadminApp')
    .controller('JoinCtrl', function ($scope, $modalInstance, $interval, mediaserver) {
        $scope.settings = {
            url :'',
            password :'',
            keepMySystem:true
        };
        $scope.systems = {
            joinSystemName : "NOT FOUND",
            systemName: "SYSTEMNAME",
            systemFound: false
        };

        mediaserver.getSettings().success(function (r) {
            $scope.systems.systemName =  r.reply.systemName;
        });


        $scope.test = function () {

            /*if (!$('#mergeSystemForm').valid()) {
                return;
            }*/


            mediaserver.pingSystem($scope.settings.url, $scope.settings.password).then(function(r){
                console.log("pingSystem",r);
                if(r.data.error!=0){
                    var errorToShow = r.data.errorString;
                    switch(errorToShow){
                        case 'FAIL':
                            errorToShow = "System is unreachable or doesn't exist.";
                            break;
                        case 'UNAUTHORIZED':
                        case 'password':
                            errorToShow = "Wrong password.";
                            break;
                        case 'INCOMPATIBLE':
                            errorToShow = "Found system has incompatible version.";
                            break;
                        case 'url':
                            errorToShow = "Wrong url.";
                            break;
                    }
                    alert("Connection failed: " + errorToShow);
                }else {
                    $scope.systems.systemFound = true;
                    $scope.systems.joinSystemName = r.data.reply.systemName;
                }
            });
        };

        $scope.join = function () {

            /*if (!$('#mergeSustemForm').valid()) {
                return;
            }*/
            $modalInstance.close({url: $scope.settings.url, password: $scope.settings.password, keepMySystem:$scope.settings.keepMySystem});
        };

        $scope.cancel = function () {
            $modalInstance.dismiss('cancel');
        };

        $scope.systemFound = false;
    });
