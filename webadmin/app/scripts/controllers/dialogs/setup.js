'use strict';

angular.module('webadminApp')
    .controller('SetupCtrl', function ($scope, mediaserver, cloudAPI, $location, $log) {
        $log.log("Initiate setup wizard (all scripts were loaded and angular started)");

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

        $scope.serverAddress = window.location.host;

        var nativeClientObject = typeof(setupDialog)=='undefined'?null:setupDialog; // Qt registered object
        var debugMode = $location.search().debug;

        var cloudAuthorized = debugMode;

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
            console.log("Wizard works in debug mode: no changes on server or portal will be made.")
            $scope.settings.presetCloudEmail = "debug@hdw.mx";
        }

        /* Funсtions for external calls (open links) */
        $scope.createAccount = function(event){
            if(nativeClientObject && nativeClientObject.openUrlInBrowser) {
                nativeClientObject.openUrlInBrowser(Config.cloud.portalRegisterUrl);
            }else{
                window.open(Config.cloud.portalRegisterUrl);
            }
            $scope.next('cloudLogin');
        };
        $scope.portalUrl = Config.cloud.portalUrl;
        $scope.openLink = function($event){
            if(nativeClientObject && nativeClientObject.openUrlInBrowser) {
                console.log($event);
                nativeClientObject.openUrlInBrowser($scope.portalUrl);
                $event.preventDefault();
            }
        };

        function tryToUpdateCredentials(){
            if(nativeClientObject && nativeClientObject.updateCredentials){
                $log.log("Send credentials to client app: " + $scope.activeLogin);
                nativeClientObject.updateCredentials ($scope.activeLogin, $scope.activePassword, $scope.cloudCreds);
            }
        }
        /* Common helpers: error handling, check current system, error handler */
        function checkMySystem(user){
            $log.log("check system configuration");

            $scope.settings.localLogin = user.name || Config.defaultLogin;

            if(debugMode) {
                $scope.next('start');// go to start
                return;
            }
            mediaserver.systemCloudInfo().then(function(data){
                $scope.settings.cloudSystemID = data.cloudSystemID;
                $scope.settings.cloudEmail = data.cloudAccountName;
                tryToUpdateCredentials();
                $log.log("System is in cloud! go to CloudSuccess");
                $scope.next('cloudSuccess');
            },function(){
                mediaserver.getSettings(true).then(function (r) {
                    $scope.serverInfo = r.data.reply;

                    if(debugMode || $scope.serverInfo.serverFlags.indexOf(Config.newServerFlag)>=0) {
                        $log.log("System is new - go to master");
                        $scope.next('start');// go to start
                    }else{
                        tryToUpdateCredentials();
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
            return mediaserver.login(login, password).then(checkMySystem,function(error){
                $log.error("Authorization on server with login " + login + " failed:");
                logMediaserverError(error);
            });
        }


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

                systems = _.sortBy(systems,function(system){
                    return system.visibleName;
                });
                $scope.discoveredUrls = _.filter(systems, function(module){
                    return module.systemName !== $scope.serverInfo.systemName;
                });
            });
        }

        var lastList = [];
        var lastSearch = null;
        $scope.getSystems = function(search){

            if(search == lastSearch){
                return lastList;
            }
            if(!search) {
                return $scope.discoveredUrls;
            }

            var systems = $scope.discoveredUrls;

            if(lastSearch){
                systems.shift();
            }

            var oldSystem = _.find($scope.discoveredUrls,function(system){
                return system.visibleName == search;
            });

            if(!oldSystem) {
                systems.unshift({
                    url: search,
                    systemName: search,
                    visibleName: search,
                    hint: search,
                    ip: search,
                    name: search
                });
            }

            lastSearch = search;
            lastList = systems;
            return lastList;
        };

        function formatError(errorToShow){
            var errorMessages = {
                'FAIL':'System is unreachable or doesn\'t exist.',
                'currentPassword':'Incorrect current password',
                'UNAUTHORIZED':'Wrong password.',
                'password':'Wrong password.',
                'INCOMPATIBLE':'Remote system has incompatible version.',
                'url':'Wrong url.',
                'SAFE_MODE':'Can\'t connect to a system. Remote system is in safe mode.',
                'CONFIGURATION_ERROR':'Can\'t connect to a system. Maybe one of the systems is in safe mode.',

                // Cloud errors:
                'notAuthorized': 'Login or password are incorrect',
                'accountNotActivated': 'Please, confirm your account first'
            };
            return errorMessages[errorToShow] || errorToShow;
        }
        function remoteErrorHandler(error){
            logMediaserverError(error);

            var errorMessage = 'Connection error (' + error.status + ')';
            if(error.data.errorString && error.data.errorString!=''){
                errorMessage = formatError(error.data.errorString);
            }
            $scope.settings.remoteError = errorMessage;
        }

        function connectToAnotherSystem(){
            $log.log("Connect to another system");
            $scope.settings.remoteError = false;
            if(debugMode){
                $scope.next('localSuccess');
                return;
            }

            $log.log("Request /api/mergeSystems ...");
            mediaserver.mergeSystems(
                $scope.settings.remoteSystem.url,
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


        function logMediaserverError(error){
            $log.error("Mediaserver error");
            if(error.data && error.data.error){
                $log.error(JSON.stringify(error.data, null, 4));
            }else{
                $log.error(JSON.stringify(error, null, 4));
            }
        }
        /* Connect to cloud section */


        function connectToCloud(preset){
            $log.log("Connect to cloud");

            if(preset){
                $scope.settings.cloudEmail = $scope.settings.presetCloudEmail;
                $scope.settings.cloudPassword = $scope.settings.presetCloudPassword;
            }
            $scope.settings.cloudError = false;
            if(debugMode){
                $scope.portalSystemLink = Config.cloud.portalSystemUrl.replace("{systemId}",'some_system_id');
                $scope.next('cloudSuccess');
                return;
            }
            function cloudErrorHandler(error)
            {
                $log.error("Cloud portal error");
                $log.error(JSON.stringify(error, null, 4));

                if(error.status === 401){
                    $log.error("Wrong login or password - show alert message");
                    $log.error(JSON.stringify(error.data, null, 4));

                    if(error.data.resultCode) {
                        $scope.settings.cloudError = formatError(error.data.resultCode);
                    }else{
                        $scope.settings.cloudError = formatError('notAuthorized');
                    }
                    return;
                }
                $log.log("Go to cloud failure step");
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
                    //2. Save settings to local server
                    console.log("Cloud portal returned success: " + JSON.stringify(message.data));

                    $scope.portalSystemLink = Config.cloud.portalSystemUrl.replace("{systemId}",message.data.id);

                    $log.log("Request /api/setupCloudSystem on mediaserver ...");
                    mediaserver.setupCloudSystem($scope.settings.systemName,
                        message.data.id,
                        message.data.authKey,
                        $scope.settings.cloudEmail).then(function(r){
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

            $log.error(errorMessage);
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
            mediaserver.setupLocalSystem($scope.settings.systemName, $scope.settings.localLogin, $scope.settings.localPassword).then(function(r){
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
        $scope.next = function(target){
            $scope.lockNextButton = true;
            if(!target) {
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
            $scope.lockNextButton = false;
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
            initFailure:{
                cancel: !!nativeClientObject || debugMode,
                next:function(){
                    initWizard();
                }
            },
            start:{
                cancel: !!nativeClientObject || debugMode,
                next: 'systemName'
            },
            systemName:{
                back: 'start',
                next: cloudAuthorized?'cloudAuthorizedIntro':'cloudIntro',
                valid: function(){
                    return required($scope.settings.systemName);
                }
            },
            cloudIntro:{
                back: 'systemName'
            },
            cloudAuthorizedIntro:{
                back: 'systemName',
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
                next: 'localLogin'
            },
            merge:{
                back: 'systemName',
                onShow: discoverSystems,
                next: connectToAnotherSystem,
                valid: function(){
                    return required($scope.settings.remoteSystem) &&
                        required($scope.settings.remoteLogin) &&
                        required($scope.settings.remotePassword);
                }
            },
            mergeSuccess:{
                finish: true
            },

            localLogin:{
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
                back:'systemName'
            }

        };

        $log.log("Wizard initiated, let's go");
        /* initiate wizard */

        function initWizard(){
            updateCredentials(Config.defaultLogin, Config.defaultPassword, false).catch(function(){
                $log.error("Couldn't run setup wizard: auth failed");
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
