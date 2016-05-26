'use strict';

angular.module('webadminApp')
    .controller('SetupCtrl', function ($scope, mediaserver, cloudAPI, $location, $log) {
        $log.log("Initiate setup wizard (all scripts were loaded and angular started)");
        $scope.Config = Config;

        if( $location.search().retry) {
            $log.log("This is second try");
        }else{
            $log.log("This is first try");
        }

        /*
            This is kind of universal wizard controller.
        */

        $scope.lockNextButton = false;
        // Common model
        $scope.settings = {
            systemName: '',

            cloudEmail: '',
            cloudPassword: '',

            localLogin: Config.defaultLogin,
            localPassword: '',
            localPasswordConfirmation:'',

            remoteSystem: '',
            remoteLogin: Config.defaultLogin,
            remotePassword:''
        };
        $scope.forms = {};

        $scope.serverAddress = window.location.host;

        var nativeClientObject = typeof(setupDialog)=='undefined'?null:setupDialog; // Qt registered object
        var debugMode = $location.search().debug;

        var cloudAuthorized = false;

        $log.log("check getCredentials from client");
        if(nativeClientObject && nativeClientObject.getCredentials){
            $log.log("request get credentials from client");
            var authObject = nativeClientObject.getCredentials();
            if (typeof authObject === 'string' || authObject instanceof String){
                $log.log("got string from client, try to decode JSON: " + authObject);
                try {
                    authObject = JSON.parse(authObject);
                }catch(a){
                    $log("could not decode JSON from string: " + authObject);
                }
            }
            $log.log("got credentials from client: " + JSON.stringify(authObject, null, 4));
            cloudAuthorized = authObject.cloudEmail && authObject.cloudPassword;
            if(cloudAuthorized){
                $scope.settings.presetCloudEmail = authObject.cloudEmail;
                $scope.settings.presetCloudPassword = authObject.cloudPassword;
            }
        }

        if(debugMode){
            $log.log("Wizard works in debug mode: no changes on server or portal will be made.");
            cloudAuthorized = true;
            $scope.settings.presetCloudEmail = "debug@hdw.mx";
        }

        /* Funсtions for external calls (open links) */
        $scope.createAccount = function(event){
            if(nativeClientObject && nativeClientObject.openUrlInBrowser) {
                nativeClientObject.openUrlInBrowser(Config.cloud.portalRegisterUrl + Config.cloud.clientSetupContext);
            }else{
                window.open(Config.cloud.portalRegisterUrl + Config.cloud.webadminSetupContext);
            }
            $scope.next('cloudLogin');
        };
        $scope.portalUrl = Config.cloud.portalUrl;
        $scope.openLink = function($event){
            if(nativeClientObject && nativeClientObject.openUrlInBrowser) {
                nativeClientObject.openUrlInBrowser(Config.cloud.portalUrl + Config.cloud.clientSetupContext);
            }else{
                window.open(Config.cloud.portalUrl + Config.cloud.webadminSetupContext);
            }
            $event.preventDefault();
        };

        function sendCredentialsToNativeClient(){
            if(nativeClientObject && nativeClientObject.updateCredentials){
                $log.log("Send credentials to client app: " + $scope.activeLogin);
                nativeClientObject.updateCredentials ($scope.activeLogin, $scope.activePassword, $scope.cloudCreds);
            }
        }

        function checkInternet(reload){

            $log.log("check internet connection");
            if(debugMode){ // Temporary skip all internet checks
                $scope.hasInternetOnServer = true;
                $scope.hasInternetOnClient = true;
                return;
            }

            mediaserver.checkInternet().then(function(hasInternetOnServer){
                $log.log("internet on server: " + $scope.hasInternetOnServer);
                $scope.hasInternetOnServer = hasInternetOnServer;
            });

            cloudAPI.checkConnection().then(function(){
                $scope.hasInternetOnClient = true;
            },function(error){
                $scope.hasInternetOnClient = false;
                if(error.data && error.data.resultCode){
                    $scope.settings.internetError = formatError(error.data.resultCode);
                    $log.log("internet on client: " + $scope.hasInternetOnClient + ", error: " + formatError(error.data.resultCode));
                }else{
                    logMediaserverError(error, "Failed to check internet on client:");
                }
            });
        }

        /* Common helpers: error handling, check current system, error handler */
        function checkMySystem(user){
            $log.log("check system configuration");

            $scope.settings.localLogin = user.name || Config.defaultLogin;

            if(debugMode) {
                checkInternet(false);
                $scope.next('start');// go to start
                return;
            }
            mediaserver.systemCloudInfo().then(function(data){
                $scope.settings.cloudSystemID = data.cloudSystemID;
                $scope.settings.cloudEmail = data.cloudAccountName;
                sendCredentialsToNativeClient();
                $log.log("System is in cloud! go to CloudSuccess");
                $scope.next('cloudSuccess');
            },function(){
                mediaserver.getModuleInformation(true).then(function (r) {
                    $scope.serverInfo = r.data.reply;

                    checkInternet(false);
                    if(debugMode || $scope.serverInfo.serverFlags.indexOf(Config.newServerFlag)>=0) {
                        $log.log("System is new - go to master");
                        $scope.next('start');// go to start
                    }else{
                        sendCredentialsToNativeClient();
                        $log.log("System is local - go to local success");
                        $scope.next('localSuccess');
                    }
                });
            });

        }
        function updateCredentials(login, password, isCloud){
            $log.log("Apply credentials: " + login);
            $scope.activeLogin = login;
            $scope.activePassword = password;
            $scope.cloudCreds = isCloud;
            var promise = mediaserver.login(login, password);
            promise.then(checkMySystem,function(error){
                logMediaserverError(error, "Authorization on server with login " + login + " failed:");
            });
            return promise;
        }


        $scope.getSystemName = function(model){
            if(model.systemName){
                return model.systemName;
            }
            return model;
        };

        /* Connect to another server section */
        function discoverSystems() {
            mediaserver.discoveredPeers().then(function (r) {
                var systems = _.map(r.data.reply, function(module)
                {
                    var system = {
                        url: module.remoteAddresses[0] + ':' + module.port,
                        systemName: module.systemName,
                        ip: module.remoteAddresses[0],
                        name: module.name
                    };

                    system.visibleName = system.systemName + ' (' + system.url + ' - ' + system.name + ')';
                    system.hint = system.url + ' (' + system.name + ')';
                    return system;
                });

                $scope.discoveredUrls = _.sortBy(systems,function(system){
                    return system.visibleName;
                });
            });
        }

        function classifyError(error){
            var errorClasses = {

                // Auth errors:
                'UNAUTHORIZED':'auth',
                'password':'auth',

                // Wrong system:
                'FAIL':'system',
                'url':'system',

                // Merge fail:
                'INCOMPATIBLE':'fail',
                'SAFE_MODE':'fail',
                'CONFIGURATION_ERROR':'fail'
            };
            return errorClasses[error] || 'fail';
        }
        function formatError(errorToShow){
            var errorMessages = {

                // Auth errors:
                'UNAUTHORIZED':'Wrong password.',
                'password':'Wrong password.',

                // Wrong system:
                'FAIL':'System is unreachable or doesn\'t exist.',
                'url':'Unable to connect to specified server.',
                'INCOMPATIBLE':'Selected system has incompatible version.',

                // Merge fail:
                'SAFE_MODE':'Can\'t connect to a system. Remote system is in safe mode.',
                'CONFIGURATION_ERROR':'Can\'t connect to a system. Maybe one of the systems is in safe mode.',



                'currentPassword':'Incorrect current password',

                // Cloud errors:
                'notAuthorized': 'Login or password are incorrect',
                'accountNotActivated': 'Please, confirm your account first',
                'unknown': 'Something went wrong'
            };
            return errorMessages[errorToShow] || errorToShow;
        }
        $scope.clearRemoteError = function(onlyAuth){

            if(onlyAuth && !$scope.settings.remoteAuthError){
                return;
            }
            $scope.settings.remoteAuthError = false;
            $scope.forms.remoteSystemForm.remoteLogin.$setValidity('system',true);
            $scope.forms.remoteSystemForm.remotePassword.$setValidity('system',true);

            $scope.settings.remoteError = null;
            $scope.settings.remoteSystemError = false;
            $scope.forms.remoteSystemForm.remoteSystemName.$setValidity('system',true);
        };
        function remoteErrorHandler(error){
            logMediaserverError(error);

            var errorMessage = 'Connection error (' + error.status + ')';
            if(error.data.errorString && error.data.errorString!=''){
                errorMessage = formatError(error.data.errorString);
            }
            $scope.settings.remoteError = errorMessage;

            switch(classifyError(error.data.errorString)){
                case 'auth':
                    $scope.settings.remoteAuthError = true;
                    $scope.forms.remoteSystemForm.remoteLogin.$setValidity('system',false);
                    $scope.forms.remoteSystemForm.remotePassword.$setValidity('system',false);
                    $scope.next('merge');
                    break;

                case 'system':
                    $scope.settings.remoteSystemError = true;
                    $scope.forms.remoteSystemForm.remoteSystemName.$setValidity('system',false);
                    $scope.next('merge');
                    break;

                default:
                    $scope.next('mergeFailure');
                    break;
            }
        }

        function connectToAnotherSystem(){
            $log.log("Connect to another system");
            var systemUrl = $scope.settings.remoteSystem.url || $scope.settings.remoteSystem;
            $scope.settings.remoteError = false;
            if(debugMode){
                $log.log("Debug mode - only ping remote system: " + systemUrl);

                mediaserver.pingSystem(
                    systemUrl,
                    $scope.settings.remoteLogin,
                    $scope.settings.remotePassword).then(function(r){
                        if(r.data.error !== 0 && r.data.error !=='0') {
                            remoteErrorHandler(r);
                            return;
                        }
                        updateCredentials( Config.defaultLogin, Config.defaultPassword).catch(remoteErrorHandler);
                    },remoteErrorHandler);
                return;
            }


            $log.log("Request /api/mergeSystems ...");
            mediaserver.mergeSystems(
                systemUrl,
                $scope.settings.remoteLogin,
                $scope.settings.remotePassword,
                Config.defaultPassword,
                false
            ).then(function(r){
                if(r.data.error !== 0 && r.data.error !=='0') {
                    remoteErrorHandler(r);
                    return;
                }

                $log.log("Mediaserver connected system to another");
                $log.log("Apply new credentials ... ");
                updateCredentials( $scope.settings.remoteLogin, $scope.settings.remotePassword).catch(remoteErrorHandler);
            },remoteErrorHandler);
        }


        function logMediaserverError(error, message){
            if(message){
                $log.log(message);
            }
            if(!error){
                $log.error("Mediaserver  - empty error");
                $scope.errorData = 'unknown';
                return;
            }
            if(error.data && error.data.error){
                $log.error("Mediaserver error: \n" + JSON.stringify(error.data, null, 4));
                $scope.errorData = JSON.stringify(error.data, null, 4);
            }else{
                $log.error("Mediaserver error: \n" + error.statusText);
                $scope.errorData = error.statusText;
            }
        }
        /* Connect to cloud section */

        $scope.clearCloudError = function(){
            $scope.settings.cloudError = false;
        };

        function connectToCloud(preset){
            $log.log("Connect to cloud");

            if(preset){
                $scope.settings.cloudEmail = $scope.settings.presetCloudEmail;
                $scope.settings.cloudPassword = $scope.settings.presetCloudPassword;
            }
            $scope.settings.cloudError = false;
            if(debugMode){
                $scope.portalSystemLink = Config.cloud.portalSystemUrl.replace("{systemId}",'some_system_id');
                $scope.portalShortLink = Config.cloud.portalShortLink;
                $scope.next('cloudSuccess');
                return;
            }

            function cloudErrorHandler(error)
            {
                $log.error("Cloud portal error: \n" + JSON.stringify(error, null, 4));
                $scope.errorData = JSON.stringify(error, null, 4);

                if(error.status === 401 || (error.data && error.data.resultCode === 'accountNotActivated')){
                    $log.log("Wrong login or password for cloud - show red message");

                    if(error.data.resultCode) {
                        $scope.settings.cloudError = formatError(error.data.resultCode);
                    }else{
                        $scope.settings.cloudError = formatError('notAuthorized');
                    }

                    releaseNext();
                    return;
                }

                $log.log("Go to cloud failure step");
                if(error.data && error.data.resultCode) {
                    $scope.settings.cloudError = formatError(error.data.resultCode);
                }else{
                    $scope.settings.cloudError = formatError('unknown');
                }

                // Do not go further here, show connection error
                $scope.next('cloudFailure');
            }
            function errorHandler(error)
            {
                logMediaserverError(error);

                $log.log("Go to cloud failure step");
                $scope.next('cloudFailure');
            }

            $log.log("Request for id and authKey on cloud portal ...");
            //1. Request auth key from cloud_db
            cloudAPI.connect( $scope.settings.systemName, $scope.settings.cloudEmail, $scope.settings.cloudPassword).then(
                function(message){
                    if(message.data.resultCode && message.data.resultCode !== 'ok'){
                        cloudErrorHandler(message);
                        return;
                    }

                    //2. Save settings to local server
                    $log.log("Cloud portal returned success: " + JSON.stringify(message.data));

                    $scope.portalSystemLink = Config.cloud.portalSystemUrl.replace("{systemId}",message.data.id);

                    $log.log("Request /api/setupCloudSystem on mediaserver ...");
                    mediaserver.setupCloudSystem($scope.settings.systemName,
                        message.data.id,
                        message.data.authKey,
                        $scope.settings.cloudEmail,
                        $scope.systemSettings
                    ).then(function(r){
                            if(r.data.error !== 0 && r.data.error !=='0') {
                                errorHandler(r.data);
                                return;
                            }

                            $log.log("Mediaserver has linked system to the cloud");
                            $log.log("Apply cloud credentials in mediaserver ... ");
                            updateCredentials( $scope.settings.cloudEmail, $scope.settings.cloudPassword, true ).catch(errorHandler);
                        },errorHandler);
                }, cloudErrorHandler);
        }

        /* Offline system section */

        function offlineErrorHandler(error){

            logMediaserverError(error);

            var errorMessage = 'Connection error (' + error.status + ')';
            if(error.data.errorString){
                errorMessage = formatError(error.data.errorString);
            }
            $scope.settings.localError = errorMessage;

            $log.log(errorMessage);
            $log.log("Go to Local Failure step");

            $scope.next('localFailure');
        }

        function initOfflineSystem(){

            $log.log("Initiate offline (local) system");

            if(debugMode){
                $scope.next('localSuccess');
                return;
            }

            $log.log("Request /api/setupLocalSystem on cloud portal ...");
            mediaserver.setupLocalSystem($scope.settings.systemName,
                $scope.settings.localLogin,
                $scope.settings.localPassword,
                $scope.systemSettings
            ).then(function(r){
                if(r.data.error !== 0 && r.data.error !=='0') {
                    offlineErrorHandler(r);
                    return;
                }

                $log.log("setupLocalSystem returened success");
                $log.log("Apply new credentials on mediaserver ...");
                updateCredentials( $scope.settings.localLogin, $scope.settings.localPassword, false).catch(offlineErrorHandler);
            },offlineErrorHandler);
        }

        /* Common wizard functions */
        $scope.stepIs = function(step){
            return $scope.step == step;
        };
        $scope.finish = function(){
            if(nativeClientObject) {
                $log.log("close dialog");
                window.close();
            }else{
                $location.path('/settings');
                window.location.reload();
            }
        };

        $scope.skip = function() {
            if ($scope.activeStep.skip) {
                $scope.next($scope.activeStep.skip);
            }else{
                $scope.next();
            }
        };
        $scope.retry = function(){
            if($scope.activeStep.retry){
                $scope.activeStep.retried = true;
                $scope.activeStep.retry();
            }else{
                $scope.back();
            }
        };
        $scope.canRetry = function(){
            return !!$scope.activeStep.retry && !$scope.activeStep.retried;
        };
        $scope.cancel = function(){
            if(nativeClientObject) {
                if(nativeClientObject.cancel){
                    nativeClientObject.cancel();
                }

                $log.log("close dialog");
                window.close();
            }
        };
        $scope.back = function(){
            $scope.next($scope.activeStep.back);
        };

        function lockNext(){
            $scope.lockNextButton = true;
        }
        function releaseNext(){
            $scope.lockNextButton = false;
        }
        $scope.next = function(target){
            lockNext();
            if(!target && target !==0) {
                var activeStep = $scope.activeStep || $scope.wizardFlow[0];
                target = activeStep.next || activeStep.finish;
            }
            if($.isFunction(target)){
                return target();
            }
            $scope.step = target;
            $scope.activeStep = $scope.wizardFlow[target];
            if($scope.activeStep.onShow){
                $scope.activeStep.onShow();
            }
            releaseNext();
        };
        $scope.cantGoNext = function(){
            if($scope.activeStep.valid){
                return !$scope.activeStep.valid();
            }
            return false;
        };

        function required(val){
            return !!val && (!val.trim || val.trim() != '');
        }

        /* Wizard workflow */

        $scope.wizardFlow = {
            0:{
            },
            start:{
                cancel: !!nativeClientObject || debugMode,
                next: 'systemName'
            },
            systemName:{
                back: 'start',
                skip: 'merge',
                next: function(){
                    if(!$scope.hasInternetOnServer){
                        $scope.next('noInternetOnServer');
                        return;
                    }

                    if(!$scope.hasInternetOnClient){
                        $scope.next('noInternetOnClient');
                        return;
                    }
                    $scope.next(cloudAuthorized?'cloudAuthorizedIntro':'cloudIntro');
                },
                valid: function(){
                    return required($scope.settings.systemName);
                }
            },
            advanced:{
                back: 'systemName',
                next: 'systemName'
            },
            noInternetOnServer:{
                retry:function(){
                    checkInternet(true);
                },
                back:'systemName',
                skip:'localLogin'
            },
            noInternetOnClient:{
                retry:function(){
                    checkInternet(true);
                },
                back:'systemName',
                skip:'localLogin'
            },
            cloudIntro:{
                back: 'systemName',
                skip: 'localLogin'
            },
            cloudAuthorizedIntro:{
                back: 'systemName',
                skip: 'localLogin',
                next: function(){
                    connectToCloud(true)
                }
            },
            cloudLogin:{
                back: cloudAuthorized?'cloudAuthorizedIntro':'cloudIntro',
                next: connectToCloud,
                valid: function(){
                    return required($scope.settings.cloudEmail) &&
                        required($scope.settings.cloudPassword);
                }
            },
            cloudSuccess:{
                finish: true
            },
            cloudFailure:{
                back: 'cloudLogin',
                skip: 'localLogin',
                retry: function(){
                    $scope.next('cloudLogin');
                }
            },
            merge:{
                back: 'systemName',
                // onShow: discoverSystems,
                next: 'mergeProcess',
                valid: function(){
                    return required($scope.settings.remoteSystem) &&
                        required($scope.settings.remoteLogin) &&
                        required($scope.settings.remotePassword);
                }
            },
            mergeProcess:{
                onShow: connectToAnotherSystem
            },
            mergeFailure:{
                back: 'merge',
                skip: 'systemName',
                retry: function(){
                    $scope.next('merge');
                }
            },

            localLogin:{
                back: cloudAuthorized?'cloudAuthorizedIntro':'cloudIntro',
                next: initOfflineSystem,
                valid: function(){
                    return required($scope.settings.localLogin) &&
                        required($scope.settings.localPassword) &&
                        required($scope.settings.localPasswordConfirmation) &&
                        $scope.settings.localPassword === $scope.settings.localPasswordConfirmation;
                }
            },
            localSuccess:{
                finish:true
            },
            localFailure:{
                back:'systemName',
                finish:true,
                retry:function(){
                    $scope.next('start');
                }
            },
            initFailure:{
                cancel: !!nativeClientObject || debugMode,
                retry: function(){
                    initWizard();
                }
            }

        };

        $log.log("Wizard initiated, let's go");
        /* initiate wizard */

        function getAdvancedSettings(){
            mediaserver.systemSettings().then(function(r){
                var systemSettings = r.data.reply.settings;
                $scope.systemSettings = {};

                for(var settingName in $scope.Config.settingsConfig){
                    if(!$scope.Config.settingsConfig[settingName].setupWizard){
                        continue;
                    }
                    $scope.systemSettings[settingName] = systemSettings[settingName];

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
        }
        function initWizard(){
            $scope.next(0);
            updateCredentials(Config.defaultLogin, Config.defaultPassword, false).then(function() {
                getAdvancedSettings();
                discoverSystems();
            },function(error){
                $log.log("Couldn't run setup wizard: auth failed");
                $log.error(error);
                if( $location.search().retry) {
                    $log.log("Second try: show error to user");
                    $scope.next("initFailure");
                }else {
                    $log.log("Reload page to try again");
                    $location.search("retry","true");
                    window.location.reload();
                }
            });
        }

        initWizard();
    });
