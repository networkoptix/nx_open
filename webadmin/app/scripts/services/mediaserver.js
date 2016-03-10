'use strict';

angular.module('webadminApp')
    .factory('mediaserver', function ($http, $modal, $q, ipCookie) {

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

        ipCookie('Authorization','Digest', { path: '/' });

        function getSettings(){
            return $http.get(proxy + '/api/moduleInformation?showAddresses=true');
        }

        var offlineDialog = null;
        var loginDialog = null;
        function offlineHandler(error){
            // Check 401 against offline
            var inlineMode = typeof(setupDialog)!='undefined'; // Qt registered object
            var isInFrame = window.self !== window.top; // If we are in frame - do not show dialog

            if(isInFrame || inlineMode){
                return; // inline mode - do not do anything
            }
            if(error.status === 401) {
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
                return;
            }
            if(error.status === 0) {
                return; // Canceled request - do nothing here
            }
            //1. recheck
            cacheModuleInfo = null;
            if(offlineDialog === null) { //Dialog is not displayed
                getSettings(true).catch(function (/*error*/) {
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
            login:function(login,password){
                // Calculate digest
                var realm = ipCookie('realm');
                var nonce = ipCookie('nonce');

                var digest = md5(login + ':' + realm + ':' + password);
                var method = md5('GET:');
                var authDigest = md5(digest + ':' + nonce + ':' + method);
                var auth = Base64.encode(login + ':' + nonce + ':' + authDigest);

                /*
                 console.log('debug auth - realm:',realm);
                 console.log('debug auth - nonce:',nonce);
                 console.log('debug auth - login:',login);
                 console.log('debug auth - password:',$scope.user.password);
                 console.log('debug auth - digest = md5(login:realm:password):',digest);
                 console.log('debug auth - method:','GET:');
                 console.log('debug auth - md5(method):',method);
                 console.log('debug auth -  authDigest = md5(digest:nonce:md5(method)):',authDigest);
                 console.log('debug auth -  auth = base64(login:nonce:authDigest):',auth);
                 */

                ipCookie('auth',auth, { path: '/' });

                var rtspmethod = md5('PLAY:');
                var rtspDigest = md5(digest + ':' + nonce + ':' + rtspmethod);
                var authRtsp = Base64.encode(login + ':' + nonce + ':' + rtspDigest);
                ipCookie('auth_rtsp',authRtsp, { path: '/' });

                //Old cookies:  // TODO: REMOVE THIS SECTION
                //ipCookie('response',auth_digest, { path: '/' });
                //ipCookie('username',login, { path: '/' });

                // Check auth again
                return this.getUser(true);
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

                    deferred.resolve({
                        cloudSystemID:cloudSystemID,
                        cloudAccountName: cloudAccount
                    });
                });
                return deferred.promise;
            },

            detachServer:function(){
                /*var defer = $q.defer();
                defer.resolve(true);
                return defer.promise;
                */
                // TODO: wait for https://networkoptix.atlassian.net/browse/VMS-2081
                return wrapPost(proxy + '/api/detachServer');
            },
            setupCloudSystem:function(systemName, systemId, authKey){
                /*var defer = $q.defer();
                function errorHandler(error){
                    defer.reject(error);
                }
                var self = this;
                this.changeSystemName(systemName).then(
                    function(message){
                        self.saveCloudSystemCredentials(message.data.systemId, message.data.authKey).
                            then(function(result){
                                defer.resolve(result);
                            }, errorHandler);
                    }, errorHandler);
                return defer.promise;
                */
                // TODO: wait for https://networkoptix.atlassian.net/browse/VMS-2081
                return wrapPost(proxy + '/api/setupCloudSystem',{
                    systemName: systemName,
                    systemId: systemId,
                    authKey: authKey
                });
            },

            setupLocalSystem:function(systemName, adminAccount, adminPassword){

                //return this.changeSystem(systemName, adminAccount, adminPassword);

                // TODO: wait for https://networkoptix.atlassian.net/browse/VMS-2081
                return wrapPost(proxy + '/api/setupLocalSystem',{
                    systemName: systemName,
                    adminAccount: adminAccount,
                    adminPassword: adminPassword
                });

            },


            changeSystemName:function(systemName){
                return wrapPost(proxy + '/api/configure?' + $.param({
                    systemName:systemName
                }));
            },
            changeSystem:function(systemName,login,password){
                return wrapPost(proxy + '/api/configure?' + $.param({
                    systemName: systemName,
                    login: login,
                    password: password
                }));
            },

            changePort: function(port) {
                return wrapPost(proxy + '/api/configure?' + $.param({
                    port:port
                }));
            },


            mergeSystems: function(url, remoteLogin, remotePassword, currentPassword, keepMySystem){
                return wrapPost(proxy + '/api/mergeSystems?' + $.param({
                    login: remoteLogin,
                    password: remotePassword,
                    currentPassword: currentPassword,
                    url: url,
                    takeRemoteSettings: !keepMySystem
                }));
            },
            pingSystem: function(url,password){
                return wrapPost(proxy + '/api/pingSystem?' + $.param({
                    password:password,
                    url:url
                }));
            },
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
            systemSettings:function(setParams){
                //return;
                if(!setParams) {
                    return wrapGet(proxy + '/api/systemSettings');
                }else{
                    var requestParams = [];
                    for(var key in setParams){
                        requestParams.push(key + '=' + setParams[key]);
                    }

                    return wrapGet(proxy + '/api/systemSettings?' + requestParams.join('&'));
                }
            },
            getSystemSettings:function(){
                return wrapGet(proxy + '/ec2/getSettings');
            },
            saveCloudSystemCredentials: function( cloudSystemId, cloudAuthKey){
                return wrapPost(proxy + '/api/saveCloudSystemCredentials',{
                    cloudSystemID:cloudSystemId,
                    cloudAuthKey:cloudAuthKey
                });
            },
            clearCloudSystemCredentials: function(){
                return wrapPost(proxy + '/api/saveCloudSystemCredentials',{
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
            }
        };
    });
