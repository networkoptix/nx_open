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
                loginDialog.result.finally(function () {
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


        function stringifyValues(object){
            if(!jQuery.isPlainObject(object) && !jQuery.isArray(object)){
                return object;
            }
            var result = jQuery.isPlainObject(object)?{}:[];
            for(var key in object){
                result[key] =  String(object[key]);
            }
            return result;
        }


        return {
            checkCurrentPassword:function(password){
                var login = $localStorage.login;
                var realm = $localStorage.realm;
                var nonce = $localStorage.nonce;
                var auth = this.digest(login, password, realm, nonce);

                if($localStorage.auth === auth){
                    return $q.when(true);
                }
                return $q.reject();
            },
            getNonce:function(login, url){
                var proxy1 = proxy;
                if(url){
                    if(url.indexOf("http:")==0 || url.indexOf("https:")==0){
                        url = url.substring(url.indexOf("//") + 2);
                    }
                    proxy1 += '/proxy/https/' + url;
                }
                return $http.get(proxy1 + '/web/api/getNonce?userName=' + login);
            },
            logout:function(){
                $localStorage.$reset();
                return $http.post(proxy + '/web/api/cookieLogout');
            },
            digest:function(login,password,realm,nonce,method){
                var digest = md5(login + ':' + realm + ':' + password);
                var method = md5((method||'GET') + ':');
                var authDigest = md5(digest + ':' + nonce + ':' + method);
                var auth = Base64.encode(login + ':' + nonce + ':' + authDigest);

                return auth;
            },
            login:function(login,password){
                login = login.toLowerCase();
                var self = this;

                function sendLogin(){
                    return self.getNonce(login).then(function(data){
                        var realm = data.data.reply.realm;
                        var nonce = data.data.reply.nonce;

                        var auth = self.digest(login, password, realm, nonce);
                        $localStorage.$reset();

                        // Check auth again - without catching errors
                        return $http.post(proxy + '/web/api/cookieLogin',{
                            auth: auth
                        }).then(function(data){
                            if(data.data.error != "0"){
                                return $q.reject(data.data);
                            }
                            $localStorage.login = login;
                            $localStorage.nonce = nonce;
                            $localStorage.realm = realm;
                            $localStorage.auth = auth;
                            return data.data.reply;
                        });
                    });
                }
                return sendLogin();
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
                if(this.hasProxy()){ // Proxy means read-only
                    var deferred = $q.defer();
                    deferred.resolve(false);
                    return deferred.promise;
                }

                return this.getCurrentUser(reload).then(function(result){
                    /*jshint bitwise: false*/
                    var hasEditServerPermission = result.data.reply.permissions.indexOf(Config.globalEditServersPermissions)>=0;
                    var hasAllResources = result.data.reply.permissions.indexOf(Config.globalAccessAllMediaPermission)>=0;
                    /*jshint bitwise: true*/
                    var isAdmin = result.data.reply.isAdmin || hasEditServerPermission;

                    var isOwner = result.data.reply.isAdmin ;

                    return {
                        isAdmin:isAdmin,
                        isOwner:isOwner,
                        name:result.data.reply.name,
                        hasAllResources:hasAllResources || isAdmin,
                        permissions:result.data.reply.permissions
                    };
                });
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
                }).then(function(r){
                    var data = r.data.reply;
                    if(!url && !Config.cloud.portalUrl) {
                        Config.cloud.portalUrl = 'https://' + data.cloudHost;
                    }

                    data.flags = {};
                    data.flags.noHDD = data.ecDbReadOnly;

                    var ips = data.remoteAddresses;
                    data.flags.noNetwork = ips.length <= 1;
                    data.flags.wrongNetwork = true;
                    for(var ip in ips){
                        if(ip == '127.0.0.1'){ // Localhost
                            continue;
                        }
                        if(ip.indexOf('169.254.') == 0){ // No DHCP address
                            continue;
                        }
                        data.flags.wrongNetwork = false;
                    }

                    data.flags.hasInternet = data.serverFlags.indexOf(Config.publicIpFlag) >= 0;

                    data.flags.newSystem = data.serverFlags.indexOf(Config.newServerFlag) >= 0
                        && ! (data.flags.noNetwork || data.flags.noHDD);

                    return r;
                });
            },
            systemCloudInfo:function(){
                return this.getSystemSettings().then(function(data){

                    var allSettings = data.data;
                    var cloudId = _.find(allSettings, function (setting) {
                        return setting.name === 'cloudSystemID';
                    });
                    var cloudSystemID = cloudId ? cloudId.value : '';

                    if (cloudSystemID.trim() === '' || cloudSystemID === '{00000000-0000-0000-0000-000000000000}') {
                        return $q.reject(null);
                    }

                    var cloudAccount = _.find(allSettings, function (setting) {
                        return setting.name === 'cloudAccountName';
                    });
                    cloudAccount = cloudAccount ? cloudAccount.value : '';

                    return{
                        cloudSystemID:cloudSystemID,
                        cloudAccountName: cloudAccount
                    };
                });
            },
            disconnectFromCloud:function(ownerLogin,ownerPassword){
                var params = ownerPassword ? {
                    password: ownerPassword,
                    login: ownerLogin
                }: null;
                return wrapPost(proxy + '/web/api/detachFromCloud',params);

            },
            restoreFactoryDefaults:function(){
                return wrapPost(proxy + '/web/api/restoreState');
            },
            setupCloudSystem:function(systemName, systemId, authKey, cloudAccountName, systemSettings){
                return wrapPost(proxy + '/web/api/setupCloudSystem',{
                    systemName: systemName,
                    cloudSystemID: systemId,
                    cloudAuthKey: authKey,
                    cloudAccountName: cloudAccountName,
                    systemSettings: stringifyValues(systemSettings)
                });
            },

            setupLocalSystem:function(systemName, adminAccount, adminPassword, systemSettings){
                return wrapPost(proxy + '/web/api/setupLocalSystem',{
                    systemName: systemName,
                    adminAccount: adminAccount,
                    password: adminPassword,
                    systemSettings: stringifyValues(systemSettings)
                });
            },


            changeSystemName:function(systemName){
                return this.systemSettings({
                    systemName: systemName
                });
            },

            changePort: function(port) {
                return wrapPost(proxy + '/web/api/configure', {
                    port:port
                });
            },


            changeAdminPassword: function(password) {
                return wrapPost(proxy + '/web/api/configure', {
                    password:password
                });
            },

            mergeSystems: function(url, remoteLogin, remotePassword, keepMySystem){
                // 1. get remote nonce
                // /proxy/http/{url}/api/getNonce
                var self = this;
                return self.getNonce(remoteLogin, url).then(function(data){
                    // 2. calculate digest
                    var realm = data.data.reply.realm;
                    var nonce = data.data.reply.nonce;
                    var getKey = self.digest(remoteLogin, remotePassword, realm, nonce, 'GET');
                    var postKey = self.digest(remoteLogin, remotePassword, realm, nonce, 'POST');

                    // 3. pass it to merge request
                    if(url.indexOf('http')!=0){
                        url = 'https://' + url;
                    }
                    return wrapPost(proxy + '/web/api/mergeSystems?',{
                        getKey: getKey,
                        postKey: postKey,
                        url: url,
                        takeRemoteSettings: !keepMySystem
                    });
                });
            },
            pingSystem: function(url, remoteLogin, remotePassword){
                var self = this;
                return self.getNonce(remoteLogin, url).then(function(data) {
                    var realm = data.data.reply.realm;
                    var nonce = data.data.reply.nonce;
                    var getKey = self.digest(remoteLogin, remotePassword, realm, nonce, 'GET');
                    var postKey = self.digest(remoteLogin, remotePassword, realm, nonce, 'POST');

                    if (url.indexOf('http') != 0) {
                        url = 'http://' + url;
                    }
                    return wrapPost(proxy + '/web/api/pingSystem?' + $.param({
                        getKey: getKey,
                        postKey: postKey,
                        url: url
                    }));
                });
            },
            restart: function() { return wrapPost(proxy + '/web/api/restart'); },
            getStorages: function(){ return wrapGet(proxy + '/web/api/storageSpace'); },
            saveStorages:function(info){return wrapPost(proxy + '/web/ec2/saveStorages',info); },
            discoveredPeers:function(){return wrapGet(proxy + '/web/api/discoveredPeers?showAddresses=true'); },
            getMediaServer: function(id){return wrapGet(proxy + '/web/ec2/getMediaServersEx?id=' + id.replace('{','').replace('}','')); },
            getMediaServers: function(){return wrapGet(proxy + '/web/ec2/getMediaServersEx'); },
            getResourceTypes:function(){return wrapGet(proxy + '/web/ec2/getResourceTypes'); },

            getLayouts:function(){return wrapGet(proxy + '/web/ec2/getLayouts'); },
            getUsers:function(){return wrapGet(proxy + '/web/ec2/getUsers'); },

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
            /*
            // This method is not used anymore
            clearCloudSystemCredentials: function(){
                return wrapPost(proxy + '/web/api/saveCloudSystemCredentials',{
                    reset: true
                });
            },
            */
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

                return this.getModuleInformation().then(function (r) {
                    // check for safe mode and new server and redirect.
                    if(r.data.reply.flags.newSystem &&
                        $location.path()!=='/advanced' && $location.path()!=='/debug'){ // Do not redirect from advanced and debug pages
                        $location.path('/setup');
                        return null;
                    }
                    return self.getUser();
                });
            },
            checkInternet:function(reload){
                return this.getModuleInformation(reload).then(function(r){
                    var serverInfo = r.data.reply;
                    return serverInfo.flags.hasInternet;
                });
            },
            createEvent:function(params){
                return wrapGet(proxy + '/web/api/createEvent?' +  $.param(params));
            },
            getCommonPasswords:function(){
                return wrapGet('commonPasswordsList.json');
            }
        };
    });
