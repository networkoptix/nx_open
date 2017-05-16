'use strict';

//http://stackoverflow.com/questions/28045427/angularjs-custom-directive-with-input-element-pass-validator-from-outside
angular.module('webadminApp')
    .directive('systemInfo', ['mediaserver','$timeout','dialogs','systemAPI',function (mediaserver,$timeout,dialogs,systemAPI) {
        return {
            restrict: 'E',
            templateUrl: Config.viewsDir + 'components/systemInfo.html',
            link:function($scope, element, attrs, ngModel){
                $scope.Config = Config;

                function checkServersIp(server,i){
                    var ips = server.networkAddresses.split(';');

                    var port = '';
                    if(ips[i].indexOf(':') < 0){
                        port = server.apiUrl.substring(server.apiUrl.lastIndexOf(':'));
                    }

                    server.apiUrl = window.location.protocol + '//' + ips[i] + port;
                    server.apiUrlFormatted = server.apiUrl.replace('http://','').replace('https://','');

                    mediaserver.pingServer(server.apiUrl).catch(function(error){
                        if(i < ips.length-1) {
                            checkServersIp(server, i + 1);
                        }
                        else {
                            server.status = L.settings.unavailable;

                            $scope.mediaServers = _.sortBy($scope.mediaServers,function(server){
                                return (server.status==='Online'?'0':'1') + server.Name + server.id;
                                // Сортировка: online->name->id
                            });

                        }
                        return false;
                    });
                }

                function pingServers() {
                    return systemAPI.getMediaServers().then(function (data) {
                        $scope.mediaServers = _.sortBy(data.data, function (server) {
                            // Set active state for server
                            server.active = $scope.settings.id.replace('{', '').replace('}', '') === server.id.replace('{', '').replace('}', '');

                            var ips = server.networkAddresses.split(';');
                            var port = '';
                            if(ips[0].indexOf(':') < 0){
                                port = server.apiUrl.substring(server.apiUrl.lastIndexOf(':'));
                            }
                            server.apiUrl = window.location.protocol + '//' + ips[0] + port;


                            return (server.status === 'Online' ? '0' : '1') + server.Name + server.id;
                            // Sorting: online->name->id
                        });
                        $timeout(function () {
                            _.each($scope.mediaServers, function (server) {
                                checkServersIp(server, 0);
                            });
                        }, 1000);
                    });
                }

                mediaserver.getModuleInformation().then(function (r) {
                    $scope.settings = r.data.reply;

                    $scope.settings.remoteAddresses = _.filter($scope.settings.remoteAddresses,function(addr){
                        return addr.indexOf('-')==-1;
                    });
                    $scope.settings.remoteAddressesDisplay = $scope.settings.remoteAddresses.join('<br>');

                    mediaserver.resolveNewSystemAndUser().then(function(user){
                        if(user === null){
                            return;
                        }
                        $scope.user = user;
                        pingServers();
                    },function(error){
                        if(error.status !== 401 && error.status !== 403) {
                            dialogs.alert(L.navigaion.cannotGetUser);
                        }
                        pingServers();
                    });
                });
            }
        };
    }]);