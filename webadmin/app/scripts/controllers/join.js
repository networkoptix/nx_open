'use strict';

angular.module('webadminApp')
    .controller('JoinCtrl', function ($scope, $modalInstance, $interval, mediaserver) {
        $scope.settings = {url :'',  password :''};

        $scope.test = function () {
            console.log("test", $scope.settings);
            mediaserver.pingSystem($scope.settings.url, $scope.settings.password).then(function(r){
                if(r.data.error!=0){
                    var errorToShow = r.data.errorString;
                    switch(errorToShow){
                        case 'FAIL':
                            errorToShow = "System is unreachable or doesn't exist.";
                            break;
                        case 'UNAUTHORIZED':
                            errorToShow = "Wrong password.";
                            break;
                        case 'INCOMPATIBLE':
                            errorToShow = "Found system has incompatible version.";
                            break;
                    }
                    alert("Connection failed. " + errorToShow);
                }else {
                    alert("Connection succeed.");
                }
            });
        };

        $scope.join = function () {
            $modalInstance.close({url: $scope.settings.url, password: $scope.settings.password});
        };

        $scope.cancel = function () {
            $modalInstance.dismiss('cancel');
        };
    });
