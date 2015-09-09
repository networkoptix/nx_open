'use strict';

angular.module('webadminApp')
    .controller('JoinCtrl', function ($scope, $modalInstance, $interval, mediaserver) {
        $scope.settings = {
            url :'',
            password :'',
            currentPassword:'',
            keepMySystem:true
        };
        $scope.systems = {
            joinSystemName : 'NOT FOUND',
            systemName: 'SYSTEMNAME',
            systemFound: false
        };


        mediaserver.getSettings().then(function (r) {
            $scope.systems.systemName =  r.data.reply.systemName;

            /*$scope.systems.discoveredUrls = _.filter($scope.systems.discoveredUrls, function(module){
                return module.systemName !== $scope.systems.systemName;
            });*/

            mediaserver.discoveredPeers().then(function (r) {
                var systems = _.map(r.data.reply, function(module)
                {
                    return {
                        url: window.location.protocol + '//' + module.remoteAddresses[0] + ':' + module.port,
                        systemName: module.systemName,
                        ip: module.remoteAddresses[0],
                        name: module.name
                    };
                });

                $scope.systems.discoveredUrls = _.filter(systems, function(module){
                    return module.systemName !== $scope.systems.systemName;
                });

            });
        });


        function errorHandler(errorToShow){
            switch(errorToShow){
                case 'FAIL':
                    errorToShow = 'System is unreachable or doesn\'t exist.';
                    break;
                case 'currentPassword':
                    errorToShow = 'Incorrect current password';
                    break;
                case 'UNAUTHORIZED':
                case 'password':
                    errorToShow = 'Wrong password.';
                    break;
                case 'INCOMPATIBLE':
                    errorToShow = 'Found system has incompatible version.';
                    break;
                case 'url':
                    errorToShow = 'Wrong url.';
                    break;
            }
            alert('Connection failed: ' + errorToShow);
        };

        $scope.test = function () {

            /*if (!$('#mergeSystemForm').valid()) {
                return;
            }*/

            mediaserver.pingSystem($scope.settings.url, $scope.settings.password).then(function(r){
                if(r.data.error!=='0'){
                    var errorToShow = r.data.errorString;
                    errorHandler(errorToShow);
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

            ediaserver.mergeSystems(settings.url,settings.password,settings.currentPassword,settings.keepMySystem).then(function(r){
                if(r.data.error!=='0'){
                    var errorToShow = r.data.errorString;
                    errorHandler(errorToShow);
                }else {
                    alert('Merge succeed.');
                    window.location.reload();
                }
            });
        };

        $scope.selectSystem = function (system){
            $scope.settings.url = system.url;
        };

        $scope.cancel = function () {
            $modalInstance.dismiss('cancel');
        };
    });
