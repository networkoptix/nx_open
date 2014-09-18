'use strict';

angular.module('webadminApp')
    .controller('RestartCtrl', function ($scope, $modalInstance, $interval, mediaserver) {
        $scope.settings = {url :'',  password :''};

        //1. request uptime
        //2. call restart function
        mediaserver.restart().then(function(){
           setTimeout(function(){
               window.location.reload();
           },15*1000);
        });
        //3. request uptime on interval
        //4. reload page or redirect to new port


        $scope.refresh = function () {
            window.location.reload();
        };
    });
