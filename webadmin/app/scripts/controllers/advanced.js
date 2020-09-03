'use strict';

angular.module('webadminApp')
    .controller('AdvancedCtrl', ['$scope', '$uibModal', '$log', 'mediaserver', '$location', 'dialogs', 'systemAPI',
    function ($scope, $uibModal, $log, mediaserver,$location, dialogs, systemAPI) {
        var isBooleanRegex = new RegExp(/enable|use(?!r)/i);

        function castBool(x) {
            return isInt(x) ? parseInt(x) > 0 : (x.toLowerCase() === 'true');
        }

        function isSettingsBool(settingValue, settingName) {
            settingValue = settingValue.toLowerCase();
            return isBooleanRegex.test(settingName) || settingValue === 'true' || settingValue === 'false';
        }

        function isInt(x) {
            return !isNaN(parseInt(x));
        }

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

        mediaserver.systemSettings({ignore:'installedUpdateInformation,targetUpdateInformation'}).then(function(r){
            $scope.systemSettings = r.data.reply.settings;

            for (var settingName in $scope.systemSettings) {
                var settingValue = $scope.systemSettings[settingName];
                if (!(settingName in $scope.Config.settingsConfig)) {
                    var type = 'text';
                    if ( isSettingsBool(settingValue, settingName)) {
                        type = 'checkbox';
                    } else if (isInt(settingValue)) {
                        type = 'number';
                    }
                    $scope.Config.settingsConfig[settingName] = {label: settingName, type: type};
                }

                if ($scope.Config.settingsConfig[settingName].type === 'number') {
                    settingValue = parseInt(settingValue);
                } else if ($scope.Config.settingsConfig[settingName].type === 'checkbox') {
                    settingValue = castBool(settingValue);
                }

                $scope.systemSettings[settingName] = settingValue;
                $scope.Config.settingsConfig[settingName].oldValue =  settingValue;
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
            $scope.storages = _.filter(r.data.reply.storages, function(storage){
                return storage.storageId != '{00000000-0000-0000-0000-000000000000}';
            });
            $scope.storages = _.sortBy($scope.storages, function(storage){
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
        
        mediaserver.logLevel().then(function(data) {
            $scope.oldLoggers = angular.copy(data.data.reply);
            $scope.loggers = data.data.reply;
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

        $scope.changeLogLevels = function(){
            var promises = [];
            for(var logger in $scope.loggers){
                if($scope.oldLoggers[logger] !== $scope.loggers[logger]){
                    promises.push(mediaserver.logLevel("", logger, $scope.loggers[logger]));
                }
            }
            
            Promise.all(promises).then(successLogLevel).catch(errorLogLevel);
        };

        $scope.restartServer = function(passPort){
            $uibModal.open({
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
