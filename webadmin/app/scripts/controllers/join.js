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

        mediaserver.discoveredPeers().success(function (r) {
            var systems = _.map(r.reply.modules, function(module)
            {
                return {
                    url: "http://" + module.remoteAddresses[0] + ":" + module.port,
                    systemName: module.systemName,
                    ip: module.remoteAddresses[0],
                    name: module.name
                };
            });

            _.filter(systems, function(module){
                return module.systemName != $scope.systems.systemName;
            });

            $scope.systems.discoveredUrls = systems;
        });

        mediaserver.getSettings().success(function (r) {
            $scope.systems.systemName =  r.reply.systemName;

            $scope.systems.discoveredUrls = _.filter($scope.systems.discoveredUrls, function(module){
                return module.systemName != $scope.systems.systemName;
            });
        });




        $scope.test = function () {

            /*if (!$('#mergeSystemForm').valid()) {
                return;
            }*/


            mediaserver.pingSystem($scope.settings.url, $scope.settings.password).then(function(r){
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

        $scope.selectSystem = function (system){
            $scope.settings.url = system.url;
        };

        $scope.cancel = function () {
            $modalInstance.dismiss('cancel');
        };
    });
