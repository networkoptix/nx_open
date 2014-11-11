
'use strict';

angular.module('webadminApp')
    .controller('InfoCtrl', function ($scope, mediaserver) {
        mediaserver.getSettings().then(function (r) {
            $scope.settings = r.data.reply;
        });


        function formatUrl(url){
            return decodeURIComponent(url.replace(/file:\/\/.+?:.+?\//gi,''));
        }

        mediaserver.getStorages().then(function (r) {
            $scope.storages = _.sortBy(r.data.reply.storages,function(storage){
                return formatUrl(storage.url);
            });

            _.each($scope.storages,function(storage){
                storage.url = formatUrl(storage.url);
            });
        });
        $scope.formatSpace = function(bytes){
            var precision = 2;
            var sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
            var posttxt = 0;
            if (bytes === 0){
                return 'n/a';
            }
            while( bytes >= 1024 ) {
                posttxt++;
                bytes = bytes / 1024;
            }
            return Number(bytes).toFixed(precision) + ' ' + sizes[posttxt];
        };

    });
