
'use strict';

angular.module('webadminApp')
    .controller('InfoCtrl', function ($scope, mediaserver) {
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

    });
