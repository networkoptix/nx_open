'use strict';

angular.module('webadminApp')
    .factory('mediaserver', function ($http, $modal) {

        var cacheModuleInfo = null;
        var cacheCurrentUser = null;

        function getSettings(){
            return $http.get('/api/moduleInformation?salt=' + (new Date()).getTime());
        }

        var offlineDialog = null;
        function offlineHandler(){
            //1. recheck
            cacheModuleInfo = null;
            if(offlineDialog === null) { //Dialog is not displayed
                getSettings(true).catch(function (error) {
                    console.log(error);// if server can't handle moduleInformation - it's offline - show dialog alike restart
                    offlineDialog = $modal.open({
                        templateUrl: 'offline_modal',
                        controller: 'OfflineCtrl'
                    });
                    offlineDialog.result.then(function (info) {//Dialog closed - means server is online
                        offlineDialog = null;
                    });
                });
            }
        }
        function wrapRequest(request){
            request.catch(offlineHandler);
            return request;
        }
        return {
            getSettings: function(url) {
                url = url || '';
                if(url === true){//force reload cache
                    cacheModuleInfo = null;
                    return getSettings();
                }
                if(url===''){// Кешируем данные о сервере, чтобы не запрашивать 10 раз
                    if(cacheModuleInfo === null){
                        cacheModuleInfo = wrapRequest(getSettings());
                    }
                    // on error - clear object to reload next time
                    return cacheModuleInfo;
                }
                return $http.get(url + '/api/moduleInformation',{
                    timeout: 3*1000
                });
            },
            saveSettings: function(systemName,port) {
                return wrapRequest($http.post('/api/configure?systemName=' + systemName + '&port=' + port));
            },
            changePassword: function(password,oldPassword) {
                return wrapRequest($http.post('/api/configure?password=' + password  + '&oldPassword=' + oldPassword));
            },
            mergeSystems: function(url,password,currentPassword,keepMySystem){
                return wrapRequest($http.post('/api/mergeSystems?password=' + password
                    + '&currentPassword=' + currentPassword
                    + '&url=' + encodeURIComponent(url)
                    + '&takeRemoteSettings=' + (!keepMySystem))); },
            pingSystem: function(url,password){return wrapRequest($http.post('/api/pingSystem?password=' + password  + '&url=' + encodeURIComponent(url))); },
            restart: function() { return wrapRequest($http.post('/api/restart')); },
            getStorages: function(){ return wrapRequest($http.get('/api/storageSpace')); },
            saveStorages:function(info){return wrapRequest($http.post('/ec2/saveStorages',info)); },
            discoveredPeers:function(){return wrapRequest($http.get('/api/discoveredPeers')); },
            getMediaServer: function(id){return wrapRequest($http.get('/ec2/getMediaServersEx?id=' + id.replace('{','').replace('}',''))); },
            getMediaServers: function(){return wrapRequest($http.get('/ec2/getMediaServersEx')); },
            getCameras:function(id){
                if(typeof(id)!=='undefined'){
                    return wrapRequest($http.get('/ec2/getCamerasEx?id=' + id.replace('{','').replace('}','')));
                }
                return wrapRequest($http.get('/ec2/getCamerasEx'));
            },
            saveMediaServer: function(info){return wrapRequest($http.post('/ec2/saveMediaServer',info)); },
            statistics:function(url){
                url = url || '';
                return $http.get(url + '/api/statistics?sault=' + (new Date()).getTime() );
            },
            getCurrentUser:function(){
                if(cacheCurrentUser === null){
                    cacheCurrentUser = wrapRequest($http.get('/api/getCurrentUser'));
                }
                return cacheCurrentUser;
            },
            getRecords:function(serverUrl,physicalId,startTime,endTime,detail,periodsType){
                var d = new Date();
                if(typeof(startTime)==='undefined'){
                    startTime = d.getTime() - 30*24*60*60*1000;
                }
                if(typeof(endTime)==='undefined'){
                    endTime = d.getTime() + 100*1000;
                }
                if(typeof(detail)==='undefined'){
                    detail = (endTime - startTime) / 1000;
                }

                if(typeof(periodsType)==='undefined'){
                    periodsType = 0;
                }

                if(serverUrl !== '/' && serverUrl !== '' && serverUrl !== null){
                    serverUrl = '/proxy/'+ serverUrl + '/';
                }
                //RecordedTimePeriods
                return $http.get(serverUrl + 'api/RecordedTimePeriods' +
                    '?physicalId=' + physicalId +
                    '&startTime=' + startTime +
                    '&endTime=' + endTime +
                    '&detail=' + detail +
                    '&periodsType=' + periodsType);
            }
        };
    });
