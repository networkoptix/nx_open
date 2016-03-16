'use strict';

angular.module('webadminApp')
    .controller('SetupCtrl', function ($scope, mediaserver, cloudAPI, $location) {
        /*
            This is kind of universal wizard controller.
        */

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
        var debugMode = !!nativeClientObject || $location.search().debug;


        /* Funсtions for external calls (open links) */
        $scope.createAccount = function(event){
            if(nativeClientObject) {
                nativeClientObject.openUrlInBrowser(Config.cloud.portalRegisterUrl);
            }else{
                window.open(Config.cloud.portalRegisterUrl);
            }
            $scope.next('cloudLogin');
        };
        $scope.portalUrl = Config.cloud.portalUrl;
        $scope.learnMore = function($event){
            if(nativeClientObject) {
                nativeClientObject.openUrlInBrowser($scope.portalUrl);
                $event.preventDefault();
            }
        };


        /* Common helpers: error handling, check current system, error handler */
        function checkMySystem(user){
            $scope.settings.localLogin = user.name || Config.defaultLogin;

            mediaserver.systemCloudInfo().then(function(data){
                $scope.settings.cloudSystemID = data.cloudSystemID;
                $scope.settings.cloudEmail = data.cloudAccountName;
                $scope.next('cloudSuccess');
            },function(){
                mediaserver.getSettings(true).then(function (r) {
                    $scope.serverInfo = r.data.reply;

                    if(debugMode || $scope.serverInfo.serverFlags.includes(Config.newServerFlag)) {
                        $scope.next('start');// go to start
                    }else{
                        $scope.next('localSuccess');
                    }
                });
            });

        }

        function updateCredentials(login,password){
            return mediaserver.login(login, password).then(checkMySystem);
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
            var errorMessage = 'Connection error (' + error.status + ')';
            if(error.data.errorString){
                errorMessage = formatError(error.data.errorString);
            }
            $scope.settings.remoteError = errorMessage;
            console.error(error);
        }

        function connectToAnotherSystem(){
            $scope.settings.remoteError = false;
            if(debugMode){
                $scope.next('localSuccess');
                return;
            }
            mediaserver.mergeSystems(
                $scope.settings.remoteSystem.url,
                $scope.settings.remoteLogin,
                $scope.settings.remotePassword,
                Config.defaultPassword,
                false
            ).then(function(r){
                if(r.data.error!=='0') {
                    remoteErrorHandler(r);
                    return;
                }
                updateCredentials( $scope.settings.remoteLogin, $scope.settings.remotePassword).catch(remoteErrorHandler);
            },remoteErrorHandler);
        }

        /* Connect to cloud section */

        function connectToCloud(){
            $scope.settings.cloudError = false;
            if(debugMode){
                $scope.next('cloudSuccess');
                return;
            }
            function cloudErrorHandler(error)
            {
                if(error.status === 401){
                    if(error.data.resultCode) {
                        $scope.settings.cloudError = formatError(error.data.resultCode);
                    }else{
                        $scope.settings.cloudError = formatError('notAuthorized');
                    }
                    return;
                }
                // Do not go further here, show connection error
                $scope.next('cloudFailure');
            }
            function errorHandler()
            {
                $scope.next('cloudFailure');
            }

            //1. Request auth key from cloud_db
            cloudAPI.connect( $scope.settings.systemName, $scope.settings.cloudEmail, $scope.settings.cloudPassword).then(
                function(message){
                    //2. Save settings to local server
                    mediaserver.setupCloudSystem($scope.settings.systemName,
                        message.data.id,
                        message.data.authKey,
                        $scope.settings.cloudEmail).then(function(){
                        updateCredentials( $scope.settings.cloudEmail, $scope.settings.cloudPassword).catch(cloudErrorHandler);
                    },errorHandler);
                }, cloudErrorHandler);
        }

        /* Offline system section */

        function offlineErrorHandler(error){
            var errorMessage = 'Connection error (' + error.status + ')';
            if(error.data.errorString){
                errorMessage = formatError(error.data.errorString);
            }
            $scope.settings.localError = errorMessage;
            $scope.next('localFailure');
        }

        function initOfflineSystem(){

            if(debugMode){
                $scope.next('localSuccess');
                return;
            }
            mediaserver.setupLocalSystem($scope.settings.systemName, $scope.settings.localLogin, $scope.settings.localPassword).then(function(r){
                if(r.data.error!=='0') {
                    offlineErrorHandler(r);
                    return;
                }
                updateCredentials( $scope.settings.localLogin, $scope.settings.localPassword).catch(offlineErrorHandler);
            },offlineErrorHandler);
        }

        /* Common wizard functions */
        $scope.stepIs = function(step){
            return $scope.step == step;
        };
        $scope.finish = function(){
            if(nativeClientObject) {
                window.close();
            }else{
                $location.path('/settings');
                window.location.reload();
            }
        };
        $scope.back = function(){
            $scope.next($scope.activeStep.back);
        };
        $scope.next = function(target){
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
                next:'start'
            },
            start:{
                next: 'systemName'
            },
            systemName:{
                back: 'start',
                next: 'cloudIntro',
                valid: function(){
                    return required($scope.settings.systemName);
                }
            },
            cloudIntro:{
                back: 'systemName'
            },
            cloudLogin:{
                back: 'cloudIntro',
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

        /* initiate wizard */

        mediaserver.getUser().then(checkMySystem,function(){
            updateCredentials(Config.defaultLogin,Config.defaultPassword);
        });
        //
    });