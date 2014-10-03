'use strict';

angular.module('webadminApp')
    .controller('RestartCtrl', function ($scope, $modalInstance, $interval, mediaserver,data) {
        $scope.url = window.location.protocol + "//" + window.location.hostname + ":"
            + data.port + window.location.pathname + window.location.search;

        $scope.state = '';
        var statisticUrl = window.location.protocol + "//" + window.location.hostname + ":"
            + data.port;

        var oldUptime = Number.MAX_VALUE;
        var serverWasDown = false;

        function reload(){
            window.location.href = $scope.url;
            window.location.reload($scope.url);
            return false;
        }

        $scope.refresh = reload;

        function pingServer(){
            mediaserver.statistics(statisticUrl).success(function(result){
                var newUptime  = result.reply.uptimeMs;

                if(newUptime < oldUptime || serverWasDown)
                {
                    $scope.state = "server is starting";
                    return reload();
                }

                $scope.state = "server is restarting";
                oldUptime = newUptime;
                setTimeout(pingServer,1000);
            }).error(function(){
                $scope.state = "server is offline";
                serverWasDown = true; // server was down once - next success should restart server
                setTimeout(pingServer,1000);
                return false;
            });

        }

        //1. Request uptime
        mediaserver.statistics().success(function(result) {
            oldUptime = result.reply.uptimeMs;

            //2. call restart function
            mediaserver.restart().then(function () {
                    pingServer();  //3. ping every second
                }
            )
        }).error(function(){
            $scope.state = "server is offline";
            setTimeout(pingServer,1000);
            return false;
        });
    });