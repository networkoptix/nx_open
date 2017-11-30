'use strict';

angular.module('webadminApp')
    .controller('AdvancedCtrl', ['$scope', '$modal', '$log', 'mediaserver', '$location', 'dialogs', 'systemAPI',
    function ($scope, $modal, $log, mediaserver,$location, dialogs, systemAPI) {


        mediaserver.getUser().then(function(user){
            if(!user.isAdmin){
                $location.path('/info'); //no admin rights - redirect
            }
        });
        mediaserver.getModuleInformation().then(function (r) {
            if(r.data.reply.flags.noHDD){
                $location.path('/info'); //readonly - redirect
                return;
            }
        });

        $scope.Config = Config;

        mediaserver.systemSettings().then(function(r){
            $scope.systemSettings = r.data.reply.settings;

            for(var settingName in $scope.systemSettings){
                if(!$scope.Config.settingsConfig[settingName]){
                    var type = 'text';
                    if( $scope.systemSettings[settingName] === true ||
                        $scope.systemSettings[settingName] === false ||
                        $scope.systemSettings[settingName] === 'true' ||
                        $scope.systemSettings[settingName] === 'false' ){
                        type = 'checkbox';
                    }
                    $scope.Config.settingsConfig[settingName] = {label:settingName,type:type};
                }

                if($scope.Config.settingsConfig[settingName].type === 'number'){
                    $scope.systemSettings[settingName] = parseInt($scope.systemSettings[settingName]);
                }
                if($scope.systemSettings[settingName] === 'true'){
                    $scope.systemSettings[settingName] = true;
                }
                if($scope.systemSettings[settingName] === 'false'){
                    $scope.systemSettings[settingName] = false;
                }

                $scope.Config.settingsConfig[settingName].oldValue =  $scope.systemSettings[settingName];
            }
        });

        $scope.saveSystemSettings = function(){
            var changedSettings = {};
            var hasChanges = false;
            for(var settingName in $scope.systemSettings){
                if($scope.Config.settingsConfig[settingName].oldValue !== $scope.systemSettings[settingName]){
                    changedSettings[settingName] = $scope.systemSettings[settingName];
                    hasChanges = true;
                }
            }
            if(hasChanges){
                mediaserver.systemSettings(changedSettings).then(function(r){
                    if(typeof(r.error) !== 'undefined' && r.error !== '0') {
                        var errorToShow = r.errorString;
                        dialogs.alert('Error: ' + errorToShow);
                    }
                    else{
                        dialogs.alert('Settings saved');
                    }
                },function(error){
                    dialogs.alert('Error: Couldn\'t save settings');
                });
            }
        };


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
                $scope.storages[i].accessible = parseInt($scope.storages[i].totalSpace) > 0;
                $scope.storages[i].reservedSpaceGb = Math.round($scope.storages[i].reservedSpace / (1024*1024*1024));
                $scope.storages[i].url = formatUrl($scope.storages[i].url);
            }

            $scope.$watch(function(){
                $scope.reduceArchiveWarning = false;
                $scope.someSelected = false;
                for(var i in $scope.storages){
                    var storage = $scope.storages[i];
                    storage.reservedSpace = storage.reservedSpaceGb * (1024*1024*1024);
                    $scope.someSelected = $scope.someSelected || storage.accessible && storage.isUsedForWriting && storage.isWritable && !storage.isBackup;

                    if(storage.accessible && storage.reservedSpace > storage.freeSpace ){
                        $scope.reduceArchiveWarning = true;
                    }

                    //storage.warning = storage.isUsedForWriting && (storage.reservedSpace <= 0 ||  storage.reservedSpace >= storage.totalSpace );
                }
            });
        });
        $scope.formatTotalSpace = function(storage){
            var totalSpace = 'Reserved' +': ' + $scope.formatSpace(storage.reservedSpace) + ", \n" +
                             'Free' +': ' + $scope.formatSpace(storage.freeSpace) + ", \n" +
                             'Occupied' +': ' + $scope.formatSpace(storage.totalSpace-storage.freeSpace) + ", \n" +
                             'Total' +': ' + $scope.formatSpace(storage.totalSpace);
            return totalSpace;
        };

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

            if(!$scope.someSelected){
                needConfirm = 'No main storage drive was selected for writing - video will not be recorded on this server. Do you want to continue?';
            }else if($scope.reduceArchiveWarning){
                needConfirm = 'Set reserved space is greater than free space left. Possible partial remove of the video footage is expected. Do you want to continue?';
            }

            function doSave(){
                mediaserver.getModuleInformation().then(function(settingsReply){
                    systemAPI.getMediaServers(settingsReply.data.reply.id.replace('{','').replace('}','')).then(function(mediaServerReply){
                        var info = mediaServerReply.data[0];
                        // Вот тут проапдейтить флаги в стореджах

                        _.each($scope.storages,function(storageinfo){
                            var storageToUpdate = _.findWhere(info.storages, {id: storageinfo.storageId});
                            if(storageToUpdate) {
                                storageToUpdate.spaceLimit = storageinfo.reservedSpace;
                                storageToUpdate.usedForWriting = storageinfo.isUsedForWriting;
                            }
                        });

                        mediaserver.saveStorages(info.storages).then(function(r){
                            if(typeof(r.error)!=='undefined' && r.error!=='0') {
                                var errorToShow = r.errorString;
                                dialogs.alert('Error: ' + errorToShow);
                            }
                            else{
                                dialogs.alert('Settings saved');
                            }
                        },function(){
                            dialogs.alert('Error: Couldn\'t save settings');
                        });
                    });
                });
            }

            if(needConfirm){
                dialogs.confirm(needConfirm).then(doSave);
            }else{
                doSave();
            }
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

        function errorLogLevel(/*error*/){
            dialogs.alert('Error while saving').finally(function(){
                window.location.reload();
            });
        }
        function successLogLevel(){
            dialogs.alert('Settings saved').finally(function(){
                window.location.reload();
            });
        }

        function changeTransactionLogLevel(){
            if($scope.transLogLevel !== $scope.oldTransLogLevel) {
                mediaserver.logLevel(3, $scope.transLogLevel).then(successLogLevel,errorLogLevel);
                return;
            }
            successLogLevel();
        }

        $scope.changeLogLevels = function(){
            if($scope.mainLogLevel !== $scope.oldMainLogLevel) {
                mediaserver.logLevel(0, $scope.mainLogLevel).then(changeTransactionLogLevel,errorLogLevel);
                return;
            }
            changeTransactionLogLevel();
        };

        $scope.restartServer = function(passPort){
            $modal.open({
                templateUrl: Config.viewsDir + 'restart.html',
                controller: 'RestartCtrl',
                resolve:{
                    port:function(){
                        return passPort?$scope.settings.port:null;
                    }
                }
            });
        };

    }]);