'use strict';

angular.module('webadminApp')
    .factory('mediaserver', function ($http, $modal, $q, $localStorage, $location, $log) {

        var cacheModuleInfo = null;
        var cacheCurrentUser = null;

        var proxy = '';
        if(location.search.indexOf('proxy=')>0){
            var params = location.search.replace('?','').split('&');
            for (var i=0;i<params.length;i++) {
                var pair = params[i].split('=');
                if(pair[0] === 'proxy'){
                    proxy = '/proxy/' + pair[1];
                    if(pair[1] === 'demo'){
                        proxy = Config.demo;
                    }
                    break;
                }
            }
        }

        function getModuleInformation(){
            var salt = (new Date().getTime()) + '_' + Math.ceil(Math.random()*1000);
            return $http.get(proxy + '/web/api/moduleInformation?showAddresses=true&salt=' + salt);
        }

        var offlineDialog = null;
        var loginDialog = null;
        function callLogin(){
            if (loginDialog === null) { //Dialog is not displayed
                loginDialog = $modal.open({
                    templateUrl: 'views/login.html',
                    keyboard:false,
                    backdrop:'static'
                });
                loginDialog.result.then(function () {
                    loginDialog = null;
                });
            }
        }
        function offlineHandler(error){
            // Check 403 against offline
            var inlineMode = typeof(setupDialog)!='undefined'; // Qt registered object
            var isInFrame = window.self !== window.top; // If we are in frame - do not show dialog

            if(isInFrame || inlineMode){
                return; // inline mode - do not do anything
            }
            if(error.status === 403 || error.status === 401) {
                callLogin();
                return;
            }
            if(error.status === 0) {
                return; // Canceled request - do nothing here
            }
            //1. recheck
            cacheModuleInfo = null;
            if(offlineDialog === null) { //Dialog is not displayed
                getModuleInformation().catch(function (/*error*/) {
                    offlineDialog = $modal.open({
                        templateUrl: 'offline_modal',
                        controller: 'OfflineCtrl',
                        keyboard:false,
                        backdrop:'static'
                    });
                    offlineDialog.result.then(function (/*info*/) {//Dialog closed - means server is online
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
            getNonce:function(){
               return $http.get(proxy + '/web/api/getNonce');
            },
            logout:function(){
                $localStorage.$reset();
                return $http.post(proxy + '/web/api/cookieLogout');
            },
            login:function(login,password){
                var deferred = $q.defer();
                var self = this;
                function reject(error){
                    deferred.reject(error);
                }

                function sendLogin(){
                    self.getNonce().then(function(data){
                        var realm = data.data.reply.realm;
                        var nonce = data.data.reply.nonce;

                        var digest = md5(login + ':' + realm + ':' + password);
                        var method = md5('GET:');
                        var authDigest = md5(digest + ':' + nonce + ':' + method);
                        var auth = Base64.encode(login + ':' + nonce + ':' + authDigest);

                        /*
                         var rtspmethod = md5('PLAY:');
                         var rtspDigest = md5(digest + ':' + nonce + ':' + rtspmethod);
                         var authRtsp = Base64.encode(login + ':' + nonce + ':' + rtspDigest);
                         */

                        $localStorage.auth = auth;

                        // Check auth again - without catching errors
                        return $http.post(proxy + '/web/api/cookieLogin',{
                            auth: auth
                        }).then(function(data){
                            deferred.resolve(data);
                        },reject);
                    },reject);
                }
                sendLogin();
                // self.logout().then(sendLogin, sendLogin);
                return deferred.promise;
            },
            url:function(){
                return proxy;
            },
            mediaDemo:function(){
                if(proxy === Config.demo){
                    return Config.demoMedia;
                }
                return false;
            },
            logUrl:function(params){
                return proxy + '/web/api/showLog' + (params||'');
            },
            authForMedia:function(){
                return $localStorage.auth;
            },
            authForRtsp:function(){
                return $localStorage.auth; // auth_rtsp
            },

            previewUrl:function(cameraPhysicalId,time,width,height){
                if( Config.allowDebugMode) { //TODO: remove this hack later!
                    return '';
                }

                return proxy + '/web/api/image' +
                    '?physicalId=' + cameraPhysicalId +
                    (width? '&width=' + width:'') +
                    (height? '&height=' + height:'') +
                    ('&time=' + (time || 'LATEST')); // mb LATEST?

            },
            hasProxy:function(){
                return proxy !=='';
            },
            getUser:function(reload){
                var deferred = $q.defer();
                if(this.hasProxy()){ // Proxy means read-only
                    deferred.resolve(false);
                }
                this.getCurrentUser(reload).then(function(result){
                    /*jshint bitwise: false*/
                    var hasEditServerPermission = result.data.reply.permissions & Config.globalEditServersPermissions;
                    /*jshint bitwise: true*/
                    var isAdmin = result.data.reply.isAdmin || hasEditServerPermission;

                    var isOwner = result.data.reply.isAdmin ;

                    deferred.resolve({
                        isAdmin:isAdmin,
                        isOwner:isOwner,
                        name:result.data.reply.name
                    });
                },function(error){
                    deferred.reject(error);
                });
                return deferred.promise;
            },
            getScripts:function(){
                return $http.get('/web/api/scriptList');
            },
            execute:function(script,mode){
                return $http.post('/web/api/execute/' + script + '?' + (mode||''));
            },
            getModuleInformation: function(url) {
                url = url || proxy;
                if(url === true){//force reload cache
                    cacheModuleInfo = null;
                    url = proxy;
                }
                if(url === proxy){// Кешируем данные о сервере, чтобы не запрашивать 10 раз
                    if(cacheModuleInfo === null){
                        cacheModuleInfo = wrapRequest(getModuleInformation());
                    }
                    // on error - clear object to reload next time
                    return cacheModuleInfo;
                }
                //Some another server
                return $http.get(url + '/web/api/moduleInformation?showAddresses=true',{
                    timeout: 3*1000
                });
            },
            systemCloudInfo:function(){
                var deferred = $q.defer();
                this.getSystemSettings().then(function(data){

                    var allSettings = data.data;
                    var cloudId = _.find(allSettings, function (setting) {
                        return setting.name === 'cloudSystemID';
                    });
                    var cloudSystemID = cloudId ? cloudId.value : '';

                    if (cloudSystemID.trim() === '' || cloudSystemID === '{00000000-0000-0000-0000-000000000000}') {
                        deferred.reject(null);
                        return;
                    }

                    var cloudAccount = _.find(allSettings, function (setting) {
                        return setting.name === 'cloudAccountName';
                    });
                    cloudAccount = cloudAccount ? cloudAccount.value : '';

                    deferred.resolve({
                        cloudSystemID:cloudSystemID,
                        cloudAccountName: cloudAccount
                    });
                });
                return deferred.promise;
            },

            detachFromSystem:function(oldPassword){
                return wrapPost(proxy + '/web/api/detachFromSystem',{
                    oldPassword:oldPassword
                });
            },
            setupCloudSystem:function(systemName, systemId, authKey, cloudAccountName, systemSettings){
                return wrapPost(proxy + '/web/api/setupCloudSystem',{
                    systemName: systemName,
                    cloudSystemID: systemId,
                    cloudAuthKey: authKey,
                    cloudAccountName: cloudAccountName,
                    systemSettings: systemSettings
                });
            },

            setupLocalSystem:function(systemName, adminAccount, adminPassword, systemSettings){
                return wrapPost(proxy + '/web/api/setupLocalSystem',{
                    systemName: systemName,
                    adminAccount: adminAccount,
                    password: adminPassword,
                    systemSettings: systemSettings
                });
            },


            changeSystemName:function(systemName){
                return wrapPost(proxy + '/web/api/configure?' + $.param({
                    systemName:systemName
                }));
            },
            changeSystem:function(systemName,login,password){
                return wrapPost(proxy + '/web/api/configure?' + $.param({
                    systemName: systemName,
                    login: login,
                    password: password
                }));
            },

            changePort: function(port) {
                return wrapPost(proxy + '/web/api/configure?' + $.param({
                    port:port
                }));
            },


            mergeSystems: function(url, remoteLogin, remotePassword, currentPassword, keepMySystem){
                if(url.indexOf('http')!=0){
                    url = 'http://' + url;
                }
                return wrapPost(proxy + '/web/api/mergeSystems?' + $.param({
                    login: remoteLogin,
                    password: remotePassword,
                    currentPassword: currentPassword,
                    url: url,
                    takeRemoteSettings: !keepMySystem
                }));
            },
            pingSystem: function(url,login,password){
                if(url.indexOf('http')!=0){
                    url = 'http://' + url;
                }
                return wrapPost(proxy + '/web/api/pingSystem?' + $.param({
                    password:password,
                    login:login,
                    url:url
                }));
            },
            restart: function() { return wrapPost(proxy + '/web/api/restart'); },
            getStorages: function(){ return wrapGet(proxy + '/web/api/storageSpace'); },
            saveStorages:function(info){return wrapPost(proxy + '/web/ec2/saveStorages',info); },
            discoveredPeers:function(){return wrapGet(proxy + '/web/api/discoveredPeers?showAddresses=true'); },
            getMediaServer: function(id){return wrapGet(proxy + '/web/ec2/getMediaServersEx?id=' + id.replace('{','').replace('}','')); },
            getMediaServers: function(){return wrapGet(proxy + '/web/ec2/getMediaServersEx'); },
            getResourceTypes:function(){return wrapGet(proxy + '/web/ec2/getResourceTypes'); },

            getLayouts:function(){return wrapGet(proxy + '/web/ec2/getLayouts'); },

            getCameras:function(id){
                if(typeof(id)!=='undefined'){
                    return wrapGet(proxy + '/web/ec2/getCamerasEx?id=' + id.replace('{','').replace('}',''));
                }
                return wrapGet(proxy + '/web/ec2/getCamerasEx');
            },
            saveMediaServer: function(info){return wrapPost(proxy + '/web/ec2/saveMediaServer',info); },
            statistics:function(url){
                url = url || proxy;
                return wrapGet(url + '/web/api/statistics?salt=' + (new Date()).getTime());
            },
            getCurrentUser:function (forcereload){
                if(cacheCurrentUser === null || forcereload){
                    cacheCurrentUser = wrapGet(proxy + '/web/api/getCurrentUser');
                }
                return cacheCurrentUser;
            },
            getTime:function(){
                return wrapGet(proxy + '/web/api/gettime');
            },
            logLevel:function(logId,level){
                return wrapGet(proxy + '/web/api/logLevel?id=' + logId + (level?'&value=' + level:''));
            },


            systemSettings:function(setParams){
                //return;
                if(!setParams) {
                    return wrapGet(proxy + '/web/api/systemSettings');
                }else{
                    var requestParams = [];
                    for(var key in setParams){
                        requestParams.push(key + '=' + setParams[key]);
                    }

                    return wrapGet(proxy + '/web/api/systemSettings?' + requestParams.join('&'));
                }
            },
            getSystemSettings:function(){
                return wrapGet(proxy + '/web/ec2/getSettings');
            },


            saveCloudSystemCredentials: function( cloudSystemId, cloudAuthKey, cloudAccountName){
                return wrapPost(proxy + '/web/api/saveCloudSystemCredentials',{
                    cloudSystemID:cloudSystemId,
                    cloudAuthKey:cloudAuthKey,
                    cloudAccountName: cloudAccountName
                });
            },
            clearCloudSystemCredentials: function(){
                return wrapPost(proxy + '/web/api/saveCloudSystemCredentials',{
                    reset: true
                });
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
                if( proxy === Config.demo){
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
                    '&flat&keepSmallChunks');
            },
            debugFunctionUrl:function(url,getParams){
                var delimeter = url.indexOf('?')>=0? '&':'?';
                return proxy + url + delimeter + $.param(getParams);
            },
            debugFunction:function(method,url,getParams,postParams){
                switch(method){
                    case 'GET':
                        return $http.get(this.debugFunctionUrl(url,getParams));
                    case 'POST':
                        return $http.post(this.debugFunctionUrl(url,getParams), postParams);

                }
            },
            resolveNewSystemAndUser:function(){
                var self = this;
                var deferred = $q.defer();

                this.getModuleInformation().then(function (r) {
                    // check for safe mode and new server and redirect.
                    if(r.data.reply.serverFlags.indexOf(Config.newServerFlag)>=0 && !r.data.reply.ecDbReadOnly &&
                        $location.path()!=='/advanced' && $location.path()!=='/debug'){ // Do not redirect from advanced and debug pages
                        $location.path('/setup');
                        deferred.resolve(null);
                        return;
                    }
                    self.getCurrentUser().then(function(data){
                        deferred.resolve(data.data.reply);
                    },function(error){
                        deferred.resolve(null);
                    });
                });

                return deferred.promise;
            },
            checkInternet:function(reload){
                var deferred = $q.defer();
                this.getModuleInformation(reload).then(function(r){
                    var serverInfo = r.data.reply;
                    deferred.resolve(serverInfo.serverFlags && serverInfo.serverFlags.indexOf(Config.publicIpFlag) >= 0);
                },function(error){
                    deferred.resolve(false);
                });
                return deferred.promise;
            }
        };
    });
