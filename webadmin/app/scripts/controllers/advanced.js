'use strict';

angular.module('webadminApp')
    .controller('AdvancedCtrl', function ($scope, $modal, $log, mediaserver) {

        $scope.storages = mediaserver.getStorages();

        $scope.storages.then(function (r) {
            $scope.storages = r.data.reply.storages;
        });

        $scope.formatSpace = function(bytes){
            var precision = 2;
            var sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
            var posttxt = 0;
            if (bytes == 0) return 'n/a';
            while( bytes >= 1024 ) {
                posttxt++;
                bytes = bytes / 1024;
            }
            return Number(bytes).toFixed(precision) + " " + sizes[posttxt];
        };

        $scope.save = function(){
            console.log("call saveSpace");
            alert("saveSpace not implemented yet");
        };

        $scope.cancel = function(){
            window.location.reload();
        };
    });