'use strict';

angular.module('webadminApp')
    .factory('mediaserver', function ($http, $modal, $q, ipCookie,$log) {

        var cacheModuleInfo = null;
        var cacheCurrentUser = null;


        var proxy = '';
        if(location.search.indexOf('proxy=')>0){
            var params = location.search.replace('?','').split('&');
            for (var i=0;i<params.length;i++) {
                var pair = params[i].split("=");
                if(pair[0] === 'proxy'){
                    proxy = '/proxy/' + pair[1];
                    if(pair[1] == 'demo'){
                        proxy = Config.demo;
                    }

                    break;
                }
            }
        }

        ipCookie('Authorization','Digest', { path: '/' });

        function getSettings(){
            return $http.get(proxy + '/api/moduleInformation?showAddresses=true');
        }

        var offlineDialog = null;
        var loginDialog = null;
        function offlineHandler(error){

            // Check 401 against offline

            if(error.status === 401) {

                var isInFrame = window.self !== window.top; // If we are in frame - do not show dialog

                if (!isInFrame && loginDialog === null) { //Dialog is not displayed
                    loginDialog = $modal.open({
                        templateUrl: 'views/login.html',
                        keyboard:false,
                        backdrop:'static'
                    });
                    loginDialog.result.then(function () {
                        loginDialog = null;
                    });
                }
                return;
            }

            //1. recheck
            cacheModuleInfo = null;
            if(offlineDialog === null) { //Dialog is not displayed
                getSettings(true).catch(function (error) {
                    console.warn ("remove hack here!!!");
                    return;

                    $log.error(error);// if server can't handle moduleInformation - it's offline - show dialog alike restart
                    offlineDialog = $modal.open({
                        templateUrl: 'offline_modal',
                        controller: 'OfflineCtrl',
                        keyboard:false,
                        backdrop:'static'
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
            url:function(){
                return proxy;
            },
            mediaDemo:function(){
                if(proxy == Config.demo){
                    return Config.demoMedia;
                }
                return false;
            },
            logUrl:function(params){
                return proxy + '/api/showLog' + (params||'');
            },
            authForMedia:function(){
                return ipCookie('authKey'); // TODO: REMOVE
                return ipCookie('auth');
            },
            authForRtsp:function(){
                return ipCookie('authKey'); // TODO: REMOVE
                return ipCookie('auth_rtsp');
            },

            previewUrl:function(cameraPhysicalId,time,width,height){
                return proxy + '/api/image' +
                    '?physicalId=' + cameraPhysicalId +
                    (width? '&width=' + width:'') +
                    (height? '&height=' + height:'') +
                    ('&time=' + (time || 'LATEST')); // mb LATEST?

            },
            hasProxy:function(){
                return proxy !=='';
            },
            checkAdmin:function(){
                var deferred = $q.defer();
                if(this.hasProxy()){ // Proxy means read-only
                    deferred.resolve(false);
                }
                this.getCurrentUser().then(function(result){
                    var isAdmin = result.data.reply.isAdmin || (result.data.reply.permissions & Config.globalEditServersPermissions);
                    deferred.resolve(isAdmin);
                });
                return deferred.promise;
            },
            getSettings: function(url) {
                url = url || proxy;
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
                //Some another server
                return $http.get(url + '/api/moduleInformation?showAddresses=true',{
                    timeout: 3*1000
                });
            },
            saveSettings: function(systemName,port) {
                return wrapRequest($http.post(proxy + '/api/configure?systemName=' + systemName + '&port=' + port));
            },
            changePassword: function(password,oldPassword) {
                return wrapRequest($http.post(proxy + '/api/configure?password=' + password  + '&oldPassword=' + oldPassword));
            },
            mergeSystems: function(url,password,currentPassword,keepMySystem){
                return wrapRequest($http.post(proxy + '/api/mergeSystems?password=' + password
                    + '&currentPassword=' + currentPassword
                    + '&url=' + encodeURIComponent(url)
                    + '&takeRemoteSettings=' + (!keepMySystem))); },
            pingSystem: function(url,password){return wrapRequest($http.post(proxy + '/api/pingSystem?password=' + password  + '&url=' + encodeURIComponent(url))); },
            restart: function() { return wrapRequest($http.post(proxy + '/api/restart')); },
            getStorages: function(){ return wrapRequest($http.get(proxy + '/api/storageSpace')); },
            saveStorages:function(info){return wrapRequest($http.post(proxy + '/ec2/saveStorages',info)); },
            discoveredPeers:function(){return wrapRequest($http.get(proxy + '/api/discoveredPeers?showAddresses=true')); },
            getMediaServer: function(id){return wrapRequest($http.get(proxy + '/ec2/getMediaServersEx?id=' + id.replace('{','').replace('}',''))); },
            getMediaServers: function(){return wrapRequest($http.get(proxy + '/ec2/getMediaServersEx')); },
            getCameras:function(id){
                if(typeof(id)!=='undefined'){
                    return wrapRequest($http.get(proxy + '/ec2/getCamerasEx?id=' + id.replace('{','').replace('}','')));
                }
                return wrapRequest($http.get(proxy + '/ec2/getCamerasEx'));
            },
            saveMediaServer: function(info){return wrapRequest($http.post(proxy + '/ec2/saveMediaServer',info)); },
            statistics:function(url){
                url = url || proxy;
                return wrapRequest($http.get(url + '/api/statistics?salt=' + (new Date()).getTime()));
            },
            getCurrentUser:function(forcereload){
                if(cacheCurrentUser === null || forcereload){
                    cacheCurrentUser = wrapRequest($http.get(proxy + '/api/getCurrentUser'));
                }
                return cacheCurrentUser;
            },
            getRecords:function(serverUrl,physicalId,startTime,endTime,detail,limit,periodsType){

                //console.log("getRecords",serverUrl,physicalId,startTime,endTime,detail,periodsType);
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

                if(typeof(limit)=='undefined'){
                    limit = Number.MAX_VALUE;
                }
                if(serverUrl !== '/' && serverUrl !== '' && serverUrl !== null){
                    serverUrl = '/proxy/'+ serverUrl + '/';
                }
                if( proxy == Config.demo){
                    serverUrl = proxy + '/';
                }
                //RecordedTimePeriods
                return  wrapRequest($http.get(serverUrl + 'api/RecordedTimePeriods' +
                    '?physicalId=' + physicalId +
                    '&startTime=' + startTime +
                    '&endTime=' + endTime +
                    '&detail=' + detail +
                    '&periodsType=' + periodsType +
                    '&limit=' + limit));
            }
        };
    });
