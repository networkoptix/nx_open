'use strict';

angular.module('webadminApp')
    .factory('mediaserver', ['$http', '$uibModal', '$q', '$localStorage', '$location', '$log', 'nativeClient', 'systemAPI',
    function ($http, $uibModal, $q, $localStorage, $location, $log, nativeClient, systemAPI) {


        if($localStorage.auth){
            systemAPI.setAuthKeys($localStorage.auth, null, $localStorage.authRtsp);
        }

        var mediaserver = {};
        var cacheModuleInfo = null;
        var cacheCurrentUser = null;

        var proxy = '';
        // Support proxy mode
        if(location.search.indexOf('proxy=')>0){
            var params = location.search.replace('?','').split('&');
            for (var i=0;i<params.length;i++) {
                var pair = params[i].split('=');
                if(pair[0] === 'proxy'){
                    proxy = '/web/proxy/' + pair[1];
                    break;
                }
            }
        }

        function getModuleInformation(){
            var salt = (new Date().getTime()) + '_' + Math.ceil(Math.random()*1000);
            return $http.get(proxy + '/web/api/moduleInformation?showAddresses=true&salt=' + salt).then(function(r){
                var data = r.data.reply;
                if(!Config.cloud.portalUrl) {
                    Config.cloud.host = data.cloudHost;
                    Config.cloud.portalUrl = 'https://' + data.cloudHost;
                    Config.cloud.systemId = data.cloudSystemId;
                    Config.protoVersion = data.protoVersion;
                    Config.currentServerId = data.id;
                }

                var ips = _.filter(data.remoteAddresses,function(address){
                    return address !== '127.0.0.1';
                });
                var wrongNetwork = true;
                for (var ip in ips) {
                    if (ips[ip].indexOf('169.254.') == 0) { // No DHCP address
                        continue;
                    }
                    wrongNetwork = false;
                    break;
                }

                data.flags = {
                    noHDD: data.ecDbReadOnly,
                    noNetwork: !ips.length,
                    wrongNetwork: wrongNetwork,
                    hasInternet: data.serverFlags.indexOf(Config.publicIpFlag) >= 0,
                    cleanSystem: data.serverFlags.indexOf(Config.newServerFlag) >= 0,
                    canSetupNetwork: data.serverFlags.indexOf(Config.iflistFlag) >= 0,
                    canSetupTime: data.serverFlags.indexOf(Config.timeCtrlFlag) >= 0
                };
                data.flags.brokenSystem = data.flags.noHDD || data.flags.noNetwork || (data.flags.wrongNetwork && !data.flags.canSetupNetwork);
                data.flags.newSystem = data.flags.cleanSystem && !data.flags.brokenSystem;
                return r;
            });
        }

        var offlineDialog = null;
        var loginDialog = null;
        function callLogin(){
            if (loginDialog === null) { //Dialog is not displayed
                loginDialog = $uibModal.open({
                    templateUrl: Config.viewsDir + 'login.html',
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
                // Try to get credentials from native client
                return nativeClient.init().then(function(){
                    return nativeClient.getCredentials().then(function(credentials) {
                        var login = credentials.localLogin;
                        var password = credentials.localPassword;
                        if(!(login && password)) {
                            return $q.reject();
                        }
                        return mediaserver.login(login, password).then(function(){
                            setTimeout(function(){
                                window.location.reload();
                            },20);
                            return true;
                        });
                    });
                }).catch(function(error){
                    $log.error(error);
                    $log.log('fall back to login dialog');
                    callLogin();
                });
            }
            if(error.status === 0) {
                return; // Canceled request - do nothing here
            }
            //1. recheck
            cacheModuleInfo = null;
            if(offlineDialog === null) { //Dialog is not displayed
                getModuleInformation().catch(function (/*error*/) {
                    offlineDialog = $uibModal.open({
                        templateUrl: Config.viewsDir + 'components/offline.html',
                        controller: 'OfflineCtrl',
                        keyboard: false,
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
        function wrapPost(url, data){
            return wrapRequest($http.post(url, data));
        }
        function wrapGet(url, data){
            var canceller = $q.defer();
            if(data){
                url += (url.indexOf('?')>0)?'&':'?';
                url += $.param(data);
            }
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


        // Special hack to make our json recognizable by fusion on mediaserver
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


        mediaserver = {
            // TODO: remove this method and all usages in 4.1 release or later. We keep it for now to simplify merges
            /*checkCurrentPassword:function(password){
                var login = $localStorage.login;
                var realm = $localStorage.realm;
                var nonce = $localStorage.nonce;
                var auth = this.digest(login, password, realm, nonce);

                if($localStorage.auth === auth){
                    return $q.when(true);
                }
                return $q.reject();
            },*/
            getNonce:function(login, url){
                var params = {
                    userName:login
                };
                if(url){
                    if(url.indexOf('http') < 0){
                        url = 'http://' + url;
                    }
                    params.url = url;
                }
                var nonceType = url ? 'getRemoteNonce' : 'getNonce';
                return $http.get(proxy + '/web/api/' + nonceType + '?' + $.param(params));
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
            debugLogin:function(login,password,realm,nonce,method){
                var digestSource = login + ':' + realm + ':' + password;
                var digestMD5 = md5(digestSource);
                var methodSource = (method||'GET') + ':';
                var methodMD5 = md5(methodSource);
                var authDigestSource = digestMD5 + ':' + nonce + ':' + methodMD5;
                var authDigestMD5 = md5(authDigestSource);
                var authSource = login + ':' + nonce + ':' + authDigestMD5;
                var authBase64 = Base64.encode(authSource);
                return {
                    alogin:login,
                    bpassword:password,
                    crealm:realm,
                    dnonce:nonce,
                    emethod:methodSource,
                    emethodMD5:methodMD5,
                    fdigestSource: digestSource,
                    gdigestMD5: digestMD5,
                    hmethodSource: methodSource,
                    imethodMD5: methodMD5,
                    jauthDigestSource: authDigestSource,
                    kauthDigestMD5: authDigestMD5,
                    lauthSource: authSource,
                    mauthBase64: authBase64
                };
            },
            login:function(login, password){
                login = login.toLowerCase();
                var self = this;

                function sendLogin(){
                    $log.log('Login1: getNonce for ' + login);
                    return self.getNonce(login).then(function(data){
                        var realm = data.data.reply.realm;
                        var nonce = data.data.reply.nonce;

                        var auth = self.digest(login, password, realm, nonce);
                        var authRtsp = self.digest(login, password, realm, nonce, 'PLAY');

                        $log.log('Login2: nonce is ' + nonce);
                        $log.log('Login2: auth is ' + auth);

                        $log.log('Login2: cookieLogin');
                        // Check auth again - without catching errors
                        return $http.post(proxy + '/web/api/cookieLogin',{
                            auth: auth
                        }).then(function(data){
                            $log.log('Login3: cookieLogin result');
                            if(data.data.error !== '0'){
                                var resHeaders = data.headers();
                                $log.log('Login3: cookieLogin failed: ' + data.data.error);
                                data.data.authResult = 'x-auth-result' in resHeaders ? resHeaders['x-auth-result'] : '';
                                return $q.reject(data.data);
                            }
                            
                            $localStorage.$reset();
                            $localStorage.login = login;
                            $localStorage.nonce = nonce;
                            $localStorage.realm = realm;
                            $localStorage.auth = auth;
                            $localStorage.authRtsp = authRtsp;
                            systemAPI.setAuthKeys(auth, null, authRtsp);
                            $log.log('Login3: cookieLogin success!');
                            return data.data.reply;
                        });
                    });
                }
                return sendLogin();
            },
            url:function(){
                return proxy;
            },
            logUrl:function(name, lines){
                return proxy + '/web/api/showLog?' + $.param({name: name, lines: lines});
            },
            authForMedia:function(){
                return $localStorage.auth;
            },
            authForRtsp:function(){
                return $localStorage.authRtsp; // auth_rtsp
            },

            hasProxy:function(){
                return proxy !=='';
            },
            getUser:function(reload){
                return this.getCurrentUser(reload).then(function(result){
                    var hasEditServerPermission = result.data.reply.permissions.indexOf(Config.globalEditServersPermissions)>=0;
                    var hasAllResources = result.data.reply.permissions.indexOf(Config.globalAccessAllMediaPermission)>=0;

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
            pingServer:function(url){
                return $http.get(url + '/web/api/moduleInformation',{
                    timeout: 3*1000
                });
            },
            getModuleInformation: function(reload) {
                if(reload === true){//force reload cache
                    cacheModuleInfo = null;
                }
                if(cacheModuleInfo === null){
                    cacheModuleInfo = wrapRequest(getModuleInformation());
                }
                // on error - clear object to reload next time
                return cacheModuleInfo;
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
            disconnectFromCloud:function(currentPassword, ownerLogin,ownerPassword){
                var params = ownerPassword ? {
                    currentPassword: currentPassword,
                    password: ownerPassword,
                    login: ownerLogin
                }: {currentPassword:currentPassword};
                return wrapPost(proxy + '/web/api/detachFromCloud',params);

            },
            disconnectFromSystem:function(currentPassword){
                return wrapPost(proxy + '/web/api/detachFromSystem',{currentPassword:currentPassword});
            },
            restoreFactoryDefaults:function(currentPassword){
                return wrapPost(proxy + '/web/api/restoreState', {
                    currentPassword: currentPassword
                });
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


            changeAdminPassword: function(newPassword, currentPassword) {
                return wrapPost(proxy + '/web/api/configure', {
                    password: newPassword,
                    currentPassword: currentPassword
                });
            },

            mergeSystems: function(url, remoteLogin, remotePassword, keepMySystem, currentPassword){
                // 1. get remote nonce
                var self = this;
                return self.getNonce(remoteLogin, url).then(function(data){
                    if(data.data.error && data.data.error!='0'){
                        return $q.reject(data);
                    }
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
                        takeRemoteSettings: !keepMySystem,
                        currentPassword: currentPassword
                    });
                });
            },
            pingSystem: function(url, remoteLogin, remotePassword){
                var self = this;
                return self.getNonce(remoteLogin, url).then(function(data) {

                    if(data.data.error && data.data.error!='0'){
                        return $q.reject(data);
                    }
                    var realm = data.data.reply.realm;
                    var nonce = data.data.reply.nonce;
                    var getKey = self.digest(remoteLogin, remotePassword, realm, nonce, 'GET');
                    var postKey = self.digest(remoteLogin, remotePassword, realm, nonce, 'POST');

                    if (url.indexOf('http') != 0) {
                        url = 'http://' + url;
                    }
                    return wrapGet(proxy + '/web/api/pingSystem', {
                        getKey: getKey,
                        postKey: postKey,
                        url: url
                    });
                },function(error){
                    return $q.reject(error);
                });
            },
            restart: function() { return wrapPost(proxy + '/web/api/restart'); },
            getStorages: function(){ return wrapGet(proxy + '/web/api/storageSpace'); },
            saveStorages:function(info){return wrapPost(proxy + '/web/ec2/saveStorages',info); },
            discoveredPeers:function(){return wrapGet(proxy + '/web/api/discoveredPeers?showAddresses=true'); },

            getLayouts:function(){return wrapGet(proxy + '/web/ec2/getLayouts'); },
            getUsers:function(){return wrapGet(proxy + '/web/ec2/getUsers'); },

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
            getTimeZones:function(){
                return wrapGet(proxy + '/web/api/getTimeZones');
            },
            logLevel:function(logId, loggerName, level) {
                var query = {};
                if(logId) {
                    query.id = logId;
                }
                if(loggerName) {
                    query.name = loggerName;
                }
                if(level) {
                    query.value = level;
                }
                return wrapGet(proxy + '/web/api/logLevel?' + $.param(query));
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

            debugFunctionUrl:function(url, getParams, credentials){
                var delimeter = url.indexOf('?')>=0? '&':'?';
                var params = '';

                credentials = credentials ? (credentials.login + ':' + credentials.password + '@') : '';
                var host = window.location.protocol + '//' +
                           credentials +
                           window.location.hostname +
                           (window.location.port ? ':' + window.location.port: '');

                if(getParams && !_.isEmpty(getParams)){
                    params = delimeter + $.param(getParams);
                }

                params = params.replace(/=API_TOOL_OPTION_SET/,''); // special code for 'option' methods

                url = proxy + url + params;
                if(url.indexOf('/')!==0){
                    url = '/' + url;
                }
                url = url.replace('//','/');
                return host + url;
            },
            debugFunction:function(method, url, getParams, postParams, binary){
                switch(method){
                    case 'GET':
                        return $http.get(this.debugFunctionUrl(url,getParams),
                                         binary?{responseType: 'arraybuffer'}:null);
                    case 'POST':
                        return $http.post(this.debugFunctionUrl(url,getParams), postParams,
                                          binary?{responseType: 'arraybuffer'}:null);

                }
            },
            resolveNewSystemAndUser:function(){
                var self = this;

                return this.getModuleInformation().then(function (r) {
                    // check for safe mode and new server and redirect.
                    if($location.path()==='/advanced' ||  $location.path()==='/debug'){ // Do not redirect from advanced and debug pages
                        return self.getUser();
                    }
                    if(r.data.reply.flags.newSystem){  // New system - redirect to setup
                        $location.path('/setup');
                        return null;
                    }
                    if(r.data.reply.flags.brokenSystem){ // No drives - redirect to settings and hide everything else
                        $location.path('/settings/system');
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
                return wrapGet(proxy + '/web/api/createEvent',params);
            },
            getCommonPasswords:function(){
                return wrapGet('commonPasswordsList.json');
            },
            getApiMethods:function(){
                return wrapGet('api.json');
            },
            getLanguages:function(){
                return wrapGet('languages.json').then(function(data){
                    return _.map(Config.supportedLanguages, function(configLang){
                        return _.filter(data.data, function(mediaServerLang){
                            return mediaServerLang.language == configLang;
                        })[0];
                    });
                });
            },
            getServerDocumentation:function(){
                return wrapGet('/api/settingsDocumentation');
            },
            networkSettings:function(settings){
                if(!settings) {
                    return wrapGet(proxy + '/web/api/iflist');
                }
                return wrapPost(proxy + '/web/api/ifconfig', settings);
            },

            timeSettings:function(dateTime, timeZone){
                if(!dateTime || !timeZone) {
                    return wrapGet(proxy + '/web/api/gettime?local');
                }
                return wrapPost(proxy + '/web/api/setTime', stringifyValues({
                    timeZoneId: timeZone,
                    dateTime: dateTime
                }));
            }
        };

        return mediaserver;
    }]);
