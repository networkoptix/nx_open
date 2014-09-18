'use strict';

angular.module('webadminApp')
    .controller('RestartCtrl', function ($scope, $modalInstance, $interval, mediaserver,data) {
        $scope.url = window.location.protocol + "//" + window.location.hostname + ":"
            + data.port + window.location.pathname + window.location.search;

        var statisticUrl = window.location.protocol + "//" + window.location.hostname + ":"
            + data.port;

        console.log($scope.url);
        var oldUptime = 0;
        var serverWasDown = false;

        function reload(){
            window.location.href = $scope.url;
            window.location.reload($scope.url);
            return false;
        }

        $scope.refresh = reload;

        function pingServer(){
            mediaserver.statistics(statisticUrl).success(function(result){
                var newUptime  = 0;//
                if(newUptime < oldUptime || serverWasDown)
                {
                    return reload();
                }
                oldUptime = newUptime;
                setTimeput(pingServer,1000);
            }).error(function(){
                console.log("server was down");
                serverWasDown = true; // server was down once - next success should restart server
                setTimeout(pingServer,1000);
                return false;
            });

        }

        //1. Request uptime
        mediaserver.statistics().then(function(result){
            oldUptime = 0;

            //2. call restart function
            mediaserver.restart().then(function(){
                pingServer();  //3. ping every second
            });
        });
    });