
'use strict';

angular.module('webadminApp')
    .controller('InfoCtrl', ['$scope', 'mediaserver', 'nativeClient', function ($scope, mediaserver, nativeClient) {
        function formatUrl(url){
            var visibleUrl = decodeURIComponent(url.replace(/file:\/\/.+?:.+?\//gi,''));
            return visibleUrl.replace(/\/\/.+?@/,'//'); // Cut login and password from url
        }
        function getStorages() {
            mediaserver.getStorages().then(function (r) {
                $scope.storages = _.sortBy(r.data.reply.storages, function (storage) {
                    storage.url = formatUrl(storage.url);
                    return storage.url;
                });
            });
        }
    
        mediaserver.logLevel().then(function (data) {
            $scope.loggers = Object.keys(data.data.reply);
        });
    
        mediaserver.getModuleInformation().then(function (r) {
            $scope.settings = r.data.reply;
        });

        mediaserver.getUser().then(function (user) {
            $scope.user = user;
            if (user.isAdmin) {
                getStorages();
            }
        });

        nativeClient.init().then(function(result){
            $scope.mode={liteClient: result.lite};
        });
    
        $scope.L = L;
        $scope.loggers = [];
        $scope.currentLogger = "MAIN";
        $scope.formatSpace = function (bytes) {
            var precision = 2;
            var sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
            var posttxt = 0;
            if (bytes === 0) {
                return 'n/a';
            }
            while (bytes >= 1024) {
                posttxt++;
                bytes = bytes / 1024;
            }
            return Number(bytes).toFixed(precision) + ' ' + sizes[posttxt];
        };
        
        $scope.getLogger = function (name) {
            $scope.logUrl = mediaserver.logUrl(name, 1000);
        };
        
        $scope.getLogger($scope.currentLogger, 1000);
    }]);
