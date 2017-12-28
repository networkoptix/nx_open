'use strict';

angular.module('webadminApp')
    .run(['$http','$templateCache', function($http,$templateCache) {
        // Preload content into cache
        $http.get(Config.viewsDir + 'dialogs/setup-inline.html', {cache: $templateCache});
    }])
    .controller('SetupCtrl', ['$scope', 'mediaserver', 'cloudAPI', '$location',
                              '$timeout', '$log', '$q', 'nativeClient', '$poll',
    function ($scope, mediaserver, cloudAPI, $location, $timeout, $log, $q, nativeClient, $poll) {
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
            chooseCloudSystem: false,
            savePassword: true,
            systemName: '',

            cloudEmail: '',
            cloudPassword: '',

            localLogin: Config.defaultLogin,
            localPassword: '',
            localPasswordConfirmation:'',

            remoteSystem: '',
            remoteLogin: '',
            remotePassword:''
        };
        $scope.forms = {};

        $scope.serverAddress = window.location.host;

        var cloudAuthorized = false;

        function getCredentialsFromClient() {
            $log.log("check getCredentials from client");
            return nativeClient.getCredentials().then(function (authObject) {
                $log.log("request get credentials from client");
                cloudAuthorized = authObject.cloudEmail && authObject.cloudPassword;
                if (cloudAuthorized) {
                    $scope.settings.chooseCloudSystem = true;
                    $scope.settings.presetCloudEmail = authObject.cloudEmail;
                    $scope.settings.presetCloudPassword = authObject.cloudPassword;
                }
            });
        }

        /* Funсtions for external calls (open links) */
        $scope.createAccount = function($event){
            nativeClient.openUrlInBrowser(Config.cloud.portalUrl + Config.cloud.portalRegisterUrl + Config.cloud.clientSetupContext,
                L.setup.createAccount, true);
            $scope.next('cloudLogin');
            $event.preventDefault();
            $event.stopPropagation();
        };
        $scope.portalUrl = Config.cloud.portalUrl;
        $scope.openLink = function($event){
            nativeClient.openUrlInBrowser(Config.cloud.portalUrl + Config.cloud.clientSetupContext,
                $event.target.title, true);
            $event.preventDefault();
            $event.stopPropagation();
        };

        function sendCredentialsToNativeClient(){
            $log.log("Send credentials to client app: " + $scope.activeLogin);
            return nativeClient.updateCredentials($scope.activeLogin, $scope.activePassword,
                $scope.cloudCreds, $scope.settings.savePassword);
        }

        function checkInternetOnServer(reload){
            $log.log("check internet connection on server");

            return mediaserver.checkInternet(reload).then(function(hasInternetOnServer){
                $scope.hasInternetOnServer = hasInternetOnServer;
                $log.log("internet on server: " + $scope.hasInternetOnServer);
                if(!hasInternetOnServer){
                    return $q.reject();
                }
            });
        }

        function checkInternetOnClient(reload){
            $log.log("check internet connection on client");
            return cloudAPI.checkConnection(reload).then(function(){
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
        function checkInternet(reload){
            $log.log("check internet connection");

            checkInternetOnServer(reload);
            return checkInternetOnClient(reload);
        }

        /* Common helpers: error handling, check current system, error handler */
        function checkMySystem(user){
            $log.log("check system configuration, current user:", user);
            if(user){
                $scope.settings.localLogin = user.name || Config.defaultLogin;
            }

            return mediaserver.systemCloudInfo().then(function(data){
                $scope.settings.cloudSystemID = data.cloudSystemID;
                $scope.settings.cloudEmail = data.cloudAccountName;
                $log.log("Got response about systemCloudInfo");
                sendCredentialsToNativeClient();
                $log.log("System is in cloud! go to CloudSuccess");
                $scope.portalSystemLink = Config.cloud.portalUrl + Config.cloud.portalSystemUrl.replace("{systemId}", $scope.settings.cloudSystemID);
                $scope.portalShortLink = Config.cloud.portalUrl;

                $scope.next('cloudSuccess');
            },function(){
                $log.log("failed to get systemCloudInfo");
                return mediaserver.getModuleInformation(true).then(function (r) {
                    $scope.serverInfo = r.data.reply;
                    $scope.settings.systemName = $scope.serverInfo.name.replace(/^Server\s/,'');
                    $scope.port = window.location.port;
                    if($scope.serverInfo.flags.canSetupNetwork){
                        mediaserver.networkSettings().then(function(r){
                            var settings = r.data.reply;
                            $scope.IP = settings[0].ipAddr;
                            $scope.serverAddress = $scope.IP + ':' + $scope.port;
                        });
                    }

                    checkInternet(false);
                    $log.log("media server flags");
                    $log.log($scope.serverInfo.flags);

                    if($scope.serverInfo.flags.brokenSystem){
                        if($scope.serverInfo.flags.noHDD){
                            $scope.settings.localError = L.setup.errorNoHDD;
                        }else{
                            //$scope.serverInfo.flags.wrongNetwork && !data.flags.canSetupNetwork;
                            //$scope.serverInfo.flags.noNetwork
                            $scope.settings.localError = L.setup.errorNoNetwork;
                        }
                        $scope.next('brokenSystem');
                        return $q.reject();
                    }

                    if($scope.serverInfo.flags.newSystem) {
                        if($scope.serverInfo.flags.canSetupNetwork) {
                            mediaserver.networkSettings().then(function (r) {
                                $scope.networkSettings = r.data.reply;
                                if($scope.serverInfo.flags.wrongNetwork){
                                    $log.log("Wrong network settings - go to setup step");
                                    $scope.next('configureWrongNetwork');// go to start
                                }
                            });
                        }
                        if($scope.serverInfo.flags.wrongNetwork){
                            $log.log("Wrong network settings - wait for network interfaces");
                            return $q.reject();
                        }
                        $log.log("System is new - go to master");
                        $scope.next('start');// go to start
                        return $q.reject();
                    }

                    sendCredentialsToNativeClient();

                    $log.log("System is local - go to local success");
                    $scope.next('localSuccess');
                    return true;
                });
            });

        }
        function updateCredentials(login, password, isCloud){
            $log.log("Apply credentials: " + login + " cloud:" + isCloud);
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
                var systems  = _.filter(r.data.reply, function(module){
                    return !module.serverFlags.indexOf(Config.newServerFlag)>=0 && module.cloudHost == Config.cloud.host;
                });

                systems = _.map(systems, function(module)
                {
                    var system = {
                        url: module.remoteAddresses[0] + ':' + module.port,
                        systemName: module.systemName,
                        ip: module.remoteAddresses[0],
                        name: module.name,
                        isNew: module.serverFlags.indexOf(Config.newServerFlag)>=0,
                        compatibleCloudHost: module.cloudHost == Config.cloud.host
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
                'CONFIGURATION_ERROR':'fail',
                'UNCONFIGURED_SYSTEM':'fail',
                'DEPENDENT_SYSTEM_BOUND_TO_CLOUD':'fail',
                'BOTH_SYSTEM_BOUND_TO_CLOUD':'fail',
                'DIFFERENT_CLOUD_HOST':'fail'
            };
            return errorClasses[error] || 'fail';
        }
        function formatError(errorToShow){
            var errorMessages = {

                // Auth errors:
                'UNAUTHORIZED':'Wrong password.',
                'password':'Wrong password.',

                // Wrong system:
                'FAIL': L.join.systemIsUnreacheble,
                'url': L.join.wrongUrl,
                'INCOMPATIBLE': L.join.incompatibleVersion,

                // Merge fail:
                'SAFE_MODE': L.join.safeMode,
                'CONFIGURATION_ERROR': L.join.configError,
                'UNCONFIGURED_SYSTEM':L.join.newSystemError,


                'DEPENDENT_SYSTEM_BOUND_TO_CLOUD': L.join.cloudError,
                'BOTH_SYSTEM_BOUND_TO_CLOUD': L.join.cloudError,
                'DIFFERENT_CLOUD_HOST':L.join.cloudHostConflict,

                'currentPassword': L.join.incorrectCurrentPassword,

                // Cloud errors:
                'notAuthorized': L.join.incorrectRemotePassword,
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
            if(!error.data){
                error.data = error;
            }
            var errorMessage = 'Connection error (' + error.status + ')';
            if(error.data && error.data.errorString && error.data.errorString!='') {
                errorMessage = formatError(error.data.errorString);

                $scope.settings.remoteError = errorMessage;

                switch (classifyError(error.data.errorString)) {
                    case 'auth':
                        $scope.settings.remoteAuthError = true;
                        $scope.forms.remoteSystemForm.remoteLogin.$setValidity('system', false);
                        $scope.forms.remoteSystemForm.remotePassword.$setValidity('system', false);
                        $scope.next('merge');
                        break;

                    case 'system':
                        $scope.settings.remoteSystemError = true;
                        $scope.forms.remoteSystemForm.remoteSystemName.$setValidity('system', false);
                        $scope.next('merge');
                        break;

                    default:
                        $scope.settings.remoteError = errorMessage;
                        if(error.data.error == 3 && error.data.errorString.indexOf('not accessible yet') > 0){
                            $scope.next('mergeTemporaryFailure');
                            break;
                        }
                        $scope.next('mergeFailure');
                        break;
                }
            }else{
                $scope.settings.remoteError = L.join.unknownError;
                $scope.next('mergeFailure');
            }
        }

        function connectToAnotherSystem(){
            $log.log("Connect to another system");
            $log.log($scope.settings.remoteSystem);

            var systemUrl = $scope.settings.remoteSystem.url || $scope.settings.remoteSystem;
            $scope.settings.remoteError = false;

            $log.log("Request /api/mergeSystems ...");
            stopCheckingIfSystemIsReady();
            return mediaserver.mergeSystems(
                systemUrl,
                $scope.settings.remoteLogin,
                $scope.settings.remotePassword,
                false
            ).then(function(r){
                if(r.data.error !== 0 && r.data.error !=='0') {
                    remoteErrorHandler(r);
                    return;
                }

                $log.log("Mediaserver connected system to another");
                return updateCredentialsAfterMerge();
            },remoteErrorHandler);
        }

        function updateCredentialsAfterMerge(){
            $log.log("Apply new credentials ... ");
            return updateCredentials($scope.settings.remoteLogin, $scope.settings.remotePassword, false).catch(mergeLoginErrorHandler);
        }


        var retries = 0;
        function mergeLoginErrorHandler(error){
            /*
            From https://networkoptix.atlassian.net/browse/VMS-5057

             You need to check temporary unavailable error code and retry perform request:
             delay between requests 1 second
             total time 15 seconds
             if very last request still contains code "temporary unauthorized" you need to show separate error message. something "System is still being merged. please try again later".


             Serverside related changes:

             result.setError(QnRestResult::CantProcessRequest, nx::network::AppInfo::cloudName() + " is not accessible yet. Please try again later.");
             +        return nx::network::http::StatusCode::ok;

             */

            retries++;
            if(retries > Config.setup.retriesForMergeCredentialsToApply){ // Several attempts
                retries = 0;
                // Show error
                return remoteErrorHandler(error);
            }
            return $timeout(updateCredentialsAfterMerge, Config.setup.pollingTimeout);
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
            $log.error(JSON.stringify(error, null, 4));
        }
        /* Connect to cloud section */

        $scope.clearCloudError = function(){
            $scope.settings.cloudError = false;
        };

        function connectToCloud(){
            $log.log("Connect to cloud");

            $scope.settings.cloudError = false;

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

                    $scope.next('cloudLogin');
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
                releaseNext();
            }

            function errorHandler(error)
            {
                logMediaserverError(error);

                $log.log("Go to cloud failure step");
                $scope.next('cloudFailure');
            }

            $log.log("Request for id and authKey on cloud portal ...");
            //1. Request auth key from cloud_db
            return cloudAPI.connect( $scope.settings.systemName, $scope.settings.cloudEmail, $scope.settings.cloudPassword).then(
                function(message) {
                    if (message.data.resultCode && message.data.resultCode !== 'ok') {
                        cloudErrorHandler(message);
                        return;
                    }

                    //2. Save settings to local server
                    $log.log("Cloud portal returned success: " + JSON.stringify(message.data));

                    $scope.portalSystemLink = Config.cloud.portalUrl + Config.cloud.portalSystemUrl.replace("{systemId}", message.data.id);

                    $log.log("Request /api/setupCloudSystem on mediaserver ...");
                    stopCheckingIfSystemIsReady();
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
                            return updateCredentials( $scope.settings.cloudEmail, $scope.settings.cloudPassword, true ).then(function(){
                                return mediaserver.getModuleInformation(true);
                            },errorHandler);
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

            $log.log("Request /api/setupLocalSystem on cloud portal ...");
            stopCheckingIfSystemIsReady();
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
            nativeClient.closeDialog().catch(function(){
                window.location.href = window.location.protocol + '//' + window.location.host;

                /*
                $location.path('/');
                setTimeout(function(){
                    window.location.reload(true);
                });
                */
            });
        };

        $scope.skip = function() {
            if ($scope.activeStep.skip) {
                $scope.next($scope.activeStep.skip, true);
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
            nativeClient.cancelDialog();
        };
        $scope.back = function(){
            $scope.next($scope.activeStep.back, true);
        };

        function lockNext(){
            $scope.lockNextButton = true;
        }
        function releaseNext(){
            $scope.lockNextButton = false;
        }
        $scope.next = function(target, ignoreValidation){
            if($scope.cantGoNext() && !ignoreValidation){
                return;
            }
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
            if($scope.activeStep && $scope.activeStep.valid){
                return !$scope.activeStep.valid();
            }
            return false;
        };

        function touchForm(form){
            angular.forEach(form.$error, function (field) {
                angular.forEach(field, function(errorField){
                    if(typeof(errorField.$touched) != 'undefined'){
                        errorField.$setTouched();
                    }else{
                        touchForm(errorField); // Embedded form - go recursive
                    }
                })
            });
        }

        /*function setFocusToInvalid(form){
            $timeout(function() {
                $("[name='" + form.$name + "']").find('.ng-invalid:visible:first').focus();
            });
        }*/

        function checkForm(form){
            touchForm(form);
            //setFocusToInvalid(form);
            return form.$valid;
        }

        function waitForReboot(){
            $scope.next(0); // Go to loading
            var poll = $poll(pingServer, Config.setup.pollingTimeout, Config.setup.firstPollingRequest);

            function pingServer(){
                return mediaserver.getModuleInformation(true).then(function(){
                    // success
                    window.location.reload();
                    $poll.cancel(poll);
                });
            }
        }

        var checkIfSystemIsReadyPoll = null;
        function checkIfSystemIsReady(){
            checkIfSystemIsReadyPoll = $poll(reCheckSystem, Config.setup.slowPollingTimeout, Config.setup.firstPollingRequest);
            function reCheckSystem(){
                return mediaserver.getModuleInformation(true).then(function(result){
                    if(!result.data.reply.flags.cleanSystem){
                        var data = result.data.reply;

                        $scope.settings.systemName = data.systemName;
                        if(data.cloudSystemID) {
                            $log.log("Suddenly the system is in cloud! go to CloudSuccess");

                            $scope.settings.cloudSystemID = data.cloudSystemID;
                            $scope.settings.cloudEmail = null;

                            $scope.portalSystemLink = Config.cloud.portalUrl + Config.cloud.portalSystemUrl.replace("{systemId}", $scope.settings.cloudSystemID);
                            $scope.portalShortLink = Config.cloud.portalUrl;
                            $scope.next("cloudSuccess");
                        }else{
                            $log.log("Suddenly the system is ready! go to localSuccess");
                            $scope.port = window.location.port;
                            $scope.next("localSuccess");
                        }

                        stopCheckingIfSystemIsReady(checkIfSystemIsReadyPoll);
                    }
                });
            }
        }
        function stopCheckingIfSystemIsReady(){
            if(checkIfSystemIsReadyPoll) {
                $poll.cancel(checkIfSystemIsReadyPoll);
            }
        }

        /*function required(val){
            return !!val && (!val.trim || val.trim() != '');
        }*/

        /* Wizard workflow */

        function initWizardFlow() {
            $scope.wizardFlow = {
                0: {},
                start: {
                    cancel: $scope.settings.thickClient,
                    noFooter: true, // Here we disable next button
                    skip: 'merge',
                    next: 'systemName'
                },
                systemName: {
                    back: 'start',
                    skip: 'merge',
                    next: 'localLogin',
                    valid: function () {
                        return checkForm($scope.forms.systemNameForm);
                    }
                },
                advanced: {
                    back: 'systemName',
                    next: 'systemName'
                },
                configureWrongNetwork:{
                    retry:function(){
                        mediaserver.networkSettings($scope.networkSettings).then(function(){
                            return mediaserver.execute('reboot').then(waitForReboot);
                        });
                    }
                },
                configureNetworkForInternet:{
                    retry:function(){
                        mediaserver.networkSettings($scope.networkSettings).then(function(){
                            return mediaserver.execute('reboot').then(waitForReboot);
                        });
                    },
                    back: 'noInternetOnServer'
                },
                noInternetOnServer: {
                    retry: function () {
                        checkInternetOnServer(true).then(function () {
                            $scope.next(cloudAuthorized ? 'cloudAuthorizedIntro' : 'cloudIntro');
                        });
                    },
                    back: 'chooseCloudOrLocal',
                    skip: 'localLogin'
                },
                noInternetOnClient: {
                    retry: function () {
                        checkInternetOnClient(true).then(function () {
                            $scope.next(cloudAuthorized ? 'cloudAuthorizedIntro' : 'cloudIntro');
                        });
                    },
                    back: 'chooseCloudOrLocal',
                    skip: 'localLogin'
                },

                chooseCloudOrLocal:{
                    back: 'systemName',
                    next: function () {
                        if(!$scope.settings.chooseCloudSystem){
                            return $scope.next('localLogin');
                        }

                        if (!$scope.hasInternetOnServer) {
                            $scope.next('noInternetOnServer');
                            return;
                        }

                        if (!$scope.hasInternetOnClient) {
                            $scope.next('noInternetOnClient');
                            return;
                        }
                        $scope.next($scope.settings.liteClient? 'cloudLogin' : (cloudAuthorized ? 'cloudAuthorizedIntro' : 'cloudIntro'));
                    }
                },


                cloudIntro: {
                    back: 'chooseCloudOrLocal',
                    skip: 'localLogin'
                },
                cloudAuthorizedIntro: {
                    back: 'chooseCloudOrLocal',
                    skip: 'localLogin',
                    next: function () {
                        $scope.settings.cloudEmail = $scope.settings.presetCloudEmail;
                        $scope.settings.cloudPassword = $scope.settings.presetCloudPassword;
                        return $scope.next('cloudProcess');
                    }
                },
                cloudLogin: {
                    back: $scope.settings.liteClient? 'chooseCloudOrLocal' : (cloudAuthorized ? 'cloudAuthorizedIntro' : 'cloudIntro'),
                    next: 'cloudProcess',
                    valid: function () {
                        return checkForm($scope.forms.cloudForm);
                    }
                },
                cloudProcess: {
                    onShow: connectToCloud
                },
                cloudSuccess: {
                    finish: true
                },
                cloudFailure: {
                    back: 'cloudLogin',
                    skip: 'localLogin',
                    retry: function () {
                        $scope.next('cloudLogin');
                    }
                },
                merge: {
                    back: 'start',
                    // onShow: discoverSystems,
                    next: 'mergeProcess',
                    valid: function () {
                        return checkForm($scope.forms.remoteSystemForm);
                    }
                },
                mergeProcess: {
                    onShow: connectToAnotherSystem
                },
                mergeFailure: {
                    back: 'merge',
                    skip: 'start',
                    retry: function () {
                        $scope.next('merge');
                    }
                },
                retryMergeCredentials:{
                    onShow: updateCredentialsAfterMerge
                },
                mergeTemporaryFailure:{
                    retry: function () {
                        $scope.next('retryMergeCredentials');
                    },
                    finish: true
                },


                localLogin: {
                    back: function () {
                        $scope.settings.localPassword = '';
                        $scope.settings.localPasswordConfirmation = '';
                        $scope.next('systemName', true);
                    },
                    next: initOfflineSystem,
                    valid: function () {
                        return checkForm($scope.forms.localForm) &&
                            $scope.settings.localPassword === $scope.settings.localPasswordConfirmation;
                    }
                },
                localSuccess: {
                    finish: true
                },
                localFailure: {
                    back: 'systemName',
                    finish: true,
                    retry: function () {
                        $scope.next('start');
                    }
                },
                initFailure: {
                    cancel: $scope.settings.thickClient,
                    retry: function () {
                        initWizard();
                    }
                },
                brokenSystem:{
                    cancel: $scope.settings.thickClient,
                    retry: function () {
                        initWizard();
                    }
                }
            };
        }

        $log.log("Wizard initiated, let's go");
        /* initiate wizard */


        function readCloudHost(){
            return mediaserver.getModuleInformation().then(function () {
                $log.log("Read cloud portal url from module information: " + Config.cloud.portalUrl);
            });
        }
        function getAdvancedSettings(){
            return mediaserver.systemSettings().then(function(r){
                var systemSettings = r.data.reply.settings;
                $scope.systemSettings = {};

                for(var settingName in $scope.Config.settingsConfig){
                    if(!$scope.Config.settingsConfig.hasOwnProperty(settingName)){
                        continue;
                    }
                    if(!$scope.Config.settingsConfig[settingName].setupWizard){
                        continue;
                    }
                    $scope.systemSettings[settingName] = systemSettings[settingName];

                    if($scope.Config.settingsConfig[settingName].type === 'checkbox' &&
                        $scope.systemSettings[settingName] === Config.undefinedValue){
                        $scope.systemSettings[settingName] = true;
                    }
                    
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
            initWizardFlow();
            $scope.next(0);

            updateCredentials(Config.defaultLogin, Config.defaultPassword, false).then(function() {
                readCloudHost();
                getAdvancedSettings();
                discoverSystems();
                checkIfSystemIsReady();
            },function(error){
                checkMySystem().then(checkIfSystemIsReady, function(){
                    $log.log("Couldn't run setup wizard: auth failed");
                    $log.error(error);
                    if( $location.search().retry) {
                        $log.log("Second try: show error to user");
                        $scope.next("initFailure");
                    }else {
                        $log.log("Reload page to try again");
                        $location.search("retry","true");
                        setTimeout(function(){
                            window.location.reload();
                        });
                    }
                });
            });
        }
        nativeClient.init().then(function(result){
            $scope.settings.thickClient = result.thick;
            $scope.settings.liteClient = result.lite;
            if($scope.settings.liteClient) {
                $('body').addClass('lite-client-mode');
            }
            $log.log("check client Thick:" + result.thick);
            $log.log("check client Lite:" + result.lite);
            return getCredentialsFromClient();
        }).finally(initWizard);
    }]);
