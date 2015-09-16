'use strict';

angular.module('webadminApp')
    .controller('AdvancedCtrl', function ($scope, $modal, $log, mediaserver,$location) {


        mediaserver.checkAdmin().then(function(isAdmin){
            if(!isAdmin){
                $location.path('/info'); //no admin rights - redirect
            }
        });

        function formatUrl(url){
            return decodeURIComponent(url.replace(/file:\/\/.+?:.+?\//gi,''));
        }

        $scope.someSelected = false;
        $scope.reduceArchiveWarning = false;

        mediaserver.getStorages().then(function (r) {
            $scope.storages = _.sortBy(r.data.reply.storages,function(storage){
                return formatUrl(storage.url);
            });
            for(var i = 0; i<$scope.storages.length;i++){
                $scope.storages[i].reservedSpaceGb = Math.round($scope.storages[i].reservedSpace / (1024*1024*1024));
                $scope.storages[i].url = formatUrl($scope.storages[i].url);
            }

            $scope.$watch(function(){
                $scope.reduceArchiveWarning = false;
                $scope.someSelected = false;
                for(var i in $scope.storages){
                    var storage = $scope.storages[i];
                    storage.reservedSpace = storage.reservedSpaceGb * (1024*1024*1024);
                    $scope.someSelected = $scope.someSelected || storage.isUsedForWriting && storage.isWritable;

                    if(storage.reservedSpace > storage.freeSpace ){
                        $scope.reduceArchiveWarning = true;
                    }

                    //storage.warning = storage.isUsedForWriting && (storage.reservedSpace <= 0 ||  storage.reservedSpace >= storage.totalSpace );
                }
            });
        });

        $scope.formatSpace = function(bytes){
            var precision = 2;
            var sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
            var posttxt = 0;
            if (bytes === 0) {
                return 'n/a';
            }
            while( bytes >= 1024 ) {
                posttxt++;
                bytes = bytes / 1024;
            }
            return Number(bytes).toFixed(precision) + ' ' + sizes[posttxt];
        };
        $scope.toGBs = function(total){
            return Math.floor(total / (1024*1024*1024));
        };

        $scope.update = function(){

        };
        $scope.save = function(){
            var needConfirm = false;
            var hasStorageForWriting;
            _.each($scope.storages,function(storageinfo){
                hasStorageForWriting = hasStorageForWriting || storageinfo.isUsedForWriting;

                if(storageinfo.reservedSpace > storageinfo.freeSpace ){
                    needConfirm = 'Set reserved space is greater than free space left. Possible partial remove of the video footage is expected. Do you want to continue?';
                }
            });

            if(!hasStorageForWriting){
                needConfirm = 'No storages were selected for writing - video will not be recorded on this server. Do you want to continue?';
            }

            if(needConfirm && !confirm(needConfirm)) {
                return;
            }

            mediaserver.getSettings().then(function(settingsReply){
                mediaserver.getMediaServer(settingsReply.data.reply.id.replace('{','').replace('}','')).then(function(mediaServerReply){
                    var info = mediaServerReply.data[0];
                    // Вот тут проапдейтить флаги в стореджах

                    _.each($scope.storages,function(storageinfo){
                        var storageToUpdate = _.findWhere(info.storages, {id: storageinfo.storageId});
                        if(storageToUpdate!==null) {
                            storageToUpdate.spaceLimit = storageinfo.reservedSpace;
                            storageToUpdate.usedForWriting = storageinfo.isUsedForWriting;
                        }
                    });

                    mediaserver.saveStorages(info.storages).then(function(r){
                        if(typeof(r.error)!=='undefined' && r.error!=='0') {
                            var errorToShow = r.errorString;
                            alert('Error: ' + errorToShow);
                        }
                        else{
                            alert('Settings saved');
                        }
                    },function(){
                        alert('Error: Couldn\'t save settings');
                    });
                });
            });
        };

        $scope.cancel = function(){
            window.location.reload();
        };

        mediaserver.logLevel(0).then(function(data){
            $scope.mainLogLevel = $scope.oldMainLogLevel = data.data.reply;
        });
        mediaserver.logLevel(3).then(function(data){
            $scope.transLogLevel = $scope.oldTransLogLevel = data.data.reply;
        });

        function errorLogLevel(error){
            alert("Error while saving");
            window.location.reload();

        }
        function successLogLevel(){
            alert("Settings saved");
            window.location.reload();
        }

        function changeTransactionLogLevel(){
            if($scope.transLogLevel != $scope.oldTransLogLevel) {
                mediaserver.logLevel(3, $scope.transLogLevel).then(successLogLevel,errorLogLevel);;
                return;
            }
            successLogLevel();
        }

        $scope.changeLogLevels = function(){
            if($scope.mainLogLevel != $scope.oldMainLogLevel) {
                mediaserver.logLevel(0, $scope.mainLogLevel).then(changeTransactionLogLevel,errorLogLevel);
                return;
            }
            changeTransactionLogLevel();
        };
    });