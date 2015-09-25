'use strict';

angular.module('webadminApp')
    .factory('mediaserver', function ($http, $modal, $q, ipCookie,$log) {

        var cacheModuleInfo = null;
        var cacheCurrentUser = null;


        var proxy = '';
        if(location.search.indexOf('proxy=')>0){
            var params = location.search.replace('?','').split('&');
            for (var i=0;i<params.length;i++) {
                var pair = params[i].split('=');
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
            if(error.status === 0) {
                return; // Canceled request - do nothing here
            }
            console.log(error);
            //1. recheck
            cacheModuleInfo = null;
            if(offlineDialog === null) { //Dialog is not displayed
                getSettings(true).catch(function (error) {
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
        function wrapPost(url,data){
            return wrapRequest($http.post(url,data));
        }
        function wrapGet(url){
            var canceller = $q.defer();
            var obj =  wrapRequest($http.get(url, { timeout: canceller.promise }));
            obj.then(function(){
                canceller = null;
            },function(){
                canceller = null;
            });
            obj.abort = function(reason){
                if(canceller) {
                    canceller.resolve('abort request: ' + reason);
                }
            };
            return obj;
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
                //return ipCookie('authKey'); // TODO: REMOVE
                return ipCookie('auth');
            },
            authForRtsp:function(){
                //return ipCookie('authKey'); // TODO: REMOVE
                return ipCookie('auth_rtsp');
            },

            previewUrl:function(cameraPhysicalId,time,width,height){
                if( Config.allowDebugMode) { //TODO: remove this hack later!
                    return '';
                }

                return proxy + '/api/image' +
                    '?physicalId=' + cameraPhysicalId +
                    (width? '&width=' + width:'') +
                    (height? '&height=' + height:'') +
                    ('&time=' + (time || 'LATEST')); // mb LATEST?

            },
            hasProxy:function(){
                return proxy !=='';
            },
            getUser:function(){
                var deferred = $q.defer();
                if(this.hasProxy()){ // Proxy means read-only
                    deferred.resolve(false);
                }
                this.getCurrentUser().then(function(result){
                    var isAdmin = result.data.reply.isAdmin || (result.data.reply.permissions & Config.globalEditServersPermissions);
                    deferred.resolve({isAdmin:isAdmin,name:result.data.reply.name});
                });
                return deferred.promise;
            },
            getScripts:function(){
                return $http.get('/api/scriptList');
            },
            execute:function(script,mode){
                return $http.post('/api/execute/' + script + '?' + (mode||''));
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
                return wrapPost(proxy + '/api/configure?systemName=' + systemName + '&port=' + port);
            },
            changePassword: function(password,oldPassword) {
                return wrapPost(proxy + '/api/configure?password=' + password  + '&oldPassword=' + oldPassword);
            },
            mergeSystems: function(url,password,currentPassword,keepMySystem){
                return wrapPost(proxy + '/api/mergeSystems?password=' + password
                    + '&currentPassword=' + currentPassword
                    + '&url=' + encodeURIComponent(url)
                    + '&takeRemoteSettings=' + (!keepMySystem)); },
            pingSystem: function(url,password){return wrapPost(proxy + '/api/pingSystem?password=' + password  + '&url=' + encodeURIComponent(url)); },
            restart: function() { return wrapPost(proxy + '/api/restart'); },
            getStorages: function(){ return wrapGet(proxy + '/api/storageSpace'); },
            saveStorages:function(info){return wrapPost(proxy + '/ec2/saveStorages',info); },
            discoveredPeers:function(){return wrapGet(proxy + '/api/discoveredPeers?showAddresses=true'); },
            getMediaServer: function(id){return wrapGet(proxy + '/ec2/getMediaServersEx?id=' + id.replace('{','').replace('}','')); },
            getMediaServers: function(){return wrapGet(proxy + '/ec2/getMediaServersEx'); },
            getResourceTypes:function(){return wrapGet(proxy + '/ec2/getResourceTypes'); },

            getLayouts:function(){return wrapGet(proxy + '/ec2/getLayouts'); },

            getCameras:function(id){
                if(typeof(id)!=='undefined'){
                    return wrapGet(proxy + '/ec2/getCamerasEx?id=' + id.replace('{','').replace('}',''));
                }
                return wrapGet(proxy + '/ec2/getCamerasEx');
            },
            saveMediaServer: function(info){return wrapPost(proxy + '/ec2/saveMediaServer',info); },
            statistics:function(url){
                url = url || proxy;
                return wrapGet(url + '/api/statistics?salt=' + (new Date()).getTime());
            },
            getCurrentUser:function(forcereload){
                if(cacheCurrentUser === null || forcereload){
                    cacheCurrentUser = wrapGet(proxy + '/api/getCurrentUser');
                }
                return cacheCurrentUser;
            },
            getTime:function(){
                return wrapGet(proxy + '/api/gettime');
            },
            logLevel:function(logId,level){
                return wrapGet(proxy + '/api/logLevel?id=' + logId + (level?'&value=' + level:''));
            },
            getRecords:function(serverUrl, physicalId, startTime, endTime, detail, limit, label, periodsType){

                //console.log('getRecords',serverUrl,physicalId,startTime,endTime,detail,periodsType);
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
                if( proxy == Config.demo){
                    serverUrl = proxy + '/';
                }
                //RecordedTimePeriods
                return  wrapGet(serverUrl + 'ec2/recordedTimePeriods' +
                    '?' + (label||'') +
                    '&physicalId=' + physicalId +
                    '&startTime=' + startTime +
                    '&endTime=' + endTime +
                    '&detail=' + detail +
                    '&periodsType=' + periodsType +
                    (limit?'&limit=' + limit:'') +
                    '&flat');
            }
        };
    });
