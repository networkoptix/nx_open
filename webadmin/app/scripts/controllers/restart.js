'use strict';

angular.module('webadminApp')
    .controller('RestartCtrl', function ($scope, $modalInstance, $interval, mediaserver,port) {

        port = port || window.location.port;
        $scope.state = '';
        var statisticUrl = window.location.protocol + '//' + window.location.hostname + ':' + port;

        $scope.url = statisticUrl + window.location.pathname + window.location.search;

        var oldUptime = Number.MAX_VALUE;
        var serverWasDown = false;

        function reload(){
            window.location.href = statisticUrl;
            return false;
        }

        $scope.refresh = reload;
        function pingServer(){
            var request = serverWasDown ? mediaserver.getModuleInformation(statisticUrl): mediaserver.statistics(statisticUrl);
            request.then(function(result){
                if(serverWasDown || result.data.reply.uptimeMs < oldUptime )
                {
                    $scope.state = L.restartDialog.serverStarting;
                    return reload();
                }

                $scope.state = L.restartDialog.serverRestarting;
                oldUptime = result.data.reply.uptimeMs;
                setTimeout(pingServer,1000);
            },function(){
                $scope.state =  L.restartDialog.serverOffline;
                serverWasDown = true; // server was down once - next success should restart server
                setTimeout(pingServer,1000);
                return false;
            });

        }

        //1. Request uptime
        mediaserver.statistics().then(function(result) {
            oldUptime = result.data.reply.uptimeMs;

            //2. call restart function
            mediaserver.restart().then(function () {
                    pingServer();  //3. ping every second
                },function () {//on error - ping
                    pingServer();  //3. ping every second
                    return false;
                }
            );
        },function(){
            $scope.state = L.restartDialog.serverOffline;
            setTimeout(pingServer,1000);
            return false;
        });
    });