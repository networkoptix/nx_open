'use strict';

angular.module('webadminApp')
    .controller('AdvancedCtrl', function ($scope, $modal, $log, mediaserver) {

        $scope.storages = mediaserver.getStorages();

        $scope.storages.then(function (r) {

            $scope.storages = r.data.reply.storages;
            for(var i in $scope.storages){
                $scope.storages[i].reservedSpaceGb = Math.round(1.*$scope.storages[i].reservedSpace / (1024*1024*1024));
            }
            console.log($scope.storages);

            $scope.$watch(function(){
                for(var i in $scope.storages){
                    var storage = $scope.storages[i];
                    storage.reservedSpace = storage.reservedSpaceGb * (1024*1024*1024);
                    storage.warning = storage.isUsedForWriting && (storage.reservedSpace <= 0 ||  storage.reservedSpace >= storage.totalSpace );
                }
                console.log($scope.storages);
            })

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
        $scope.toGBs = function(total){
            return    Math.floor(total / (1024*1024*1024));
        }
        $scope.save = function(){
            console.log("call saveSpace");
            alert("saveSpace not implemented yet");
        };

        $scope.cancel = function(){
            window.location.reload();
        };
    });