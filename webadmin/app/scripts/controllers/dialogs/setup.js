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

            localLogin: 'admin',
            localPassword: '',
            localPasswordConfirmation:'',

            remoteSystem: '',
            remoteLogin:'',
            remotePassword:''
        };

        $scope.serverAddress = window.location.host;

        var nativeClientObject = setupDialog; // Qt registered object
        var debugMode = !!nativeClientObject;

        $scope.stepIs = function(step){
            return $scope.step == step;
        };
        $scope.finish = function(){
            $scope.next('start');
            if(nativeClientObject) {
                window.close();
            }else{
                $location.path("/settings");
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


        function checkMySystem(){
            mediaserver.getSettings().then(function (r) {
                $scope.serverInfo = r.data.reply;
                $scope.next('start');// go to start
                // If system is not new - go to success
            });
        }

        function errorHandler(error){
            alert("error happened");
            console.error(error);
        }

        function updateCredentials(login,password){
            mediaserver.login(login,password).then(checkMySystem,
                function(error){
                    console.error(error);
                });
        }

        /* Connect to another server section */
        function discoverSystems() {
            mediaserver.discoveredPeers().then(function (r) {
                var systems = _.map(r.data.reply, function(module)
                {
                    return {
                        url: module.remoteAddresses[0] + ':' + module.port,
                        systemName: module.systemName,
                        ip: module.remoteAddresses[0],
                        name: module.name
                    };
                });

                console.log(r.data.reply, systems);

                $scope.discoveredUrls = systems;
                _.filter(systems, function(module){
                    console.log(systemName);
                    return module.systemName !== $scope.serverInfo.systemName;
                });
            });
        }

        $scope.selectSystem = function (system){
            $scope.settings.remoteSystem = system.systemName;
            $scope.settings.remoteSystemHint = system.url + ' (' + system.name + ')';
            $scope.settings.selectedSystem = system;
        };

        $scope.findSystem = function(){
            $scope.settings.selectedSystem = null;
            $scope.settings.remoteSystemHint = '';
        };

        function connectToAnotherSystem(){

            if(debugMode){
                $scope.next('mergeSuccess');
                return;
            }
            mediaserver.mergeSystems(
                $scope.settings.selectedSystem.url,
                $scope.settings.remoteLogin,
                $scope.settings.remotePassword,
                'admin',
                false
            ).then(function(r){
                if(r.data.error!=='0') {
                    var errorToShow = errorHandler(r.data.errorString);
                    if (errorToShow) {
                        alert('Merge failed: ' + errorToShow);
                        return;
                    }
                }
                updateCredentials( $scope.settings.remoteLogin, $scope.settings.remotePassword);

                $scope.next('mergeSuccess');
            });
        }


        /* Connect to cloud section */

        function connectToCloud(){

            if(debugMode){
                $scope.next('cloudSuccess');
                return;
            }
            function errorHandler()
            {
                $scope.next('cloudFailure');
            }

            //1. Request auth key from cloud_db
            cloudAPI.connect( $scope.settings.systemName, $scope.settings.cloudEmail, $scope.settings.cloudPassword).then(
                function(message){
                    //2. Save settings to local server
                    mediaserver.setupCloudSystem($scope.settings.systemName, message.data.systemId, message.data.authKey).then(function(){
                        updateCredentials( $scope.settings.cloudEmail, $scope.settings.cloudPassword);
                        $scope.next('cloudSuccess');
                    },errorHandler);
                }, errorHandler);
        }

        /* Offline system section */

        function initOfflineSystem(){

            if(debugMode){
                $scope.next('localSuccess');
                return;
            }
            mediaserver.setupCloudSystem($scope.settings.systemName, $scope.settings.localLogin, $scope.settings.localPassword).then(function(){
                updateCredentials( $scope.settings.localLogin, $scope.settings.localPassword);
                $scope.next('localSuccess');
            },function(){
                $scope.next('cloudFailure');
            });
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
                next: 'cloudIntro'
            },
            cloudIntro:{
                back: 'systemName'
            },
            cloudLogin:{
                back: 'cloudIntro',
                next: connectToCloud
            },
            cloudSuccess:{
                finish: true
            },
            cloudFailure:{
                back: 'cloudLogin',
                next: 'localLogin'
            },
            merge:{
                onShow: discoverSystems,
                next: connectToAnotherSystem
            },
            mergeSuccess:{
                finish: true
            },

            localLogin:{
                next: initOfflineSystem
            },
            localSuccess:{
                finish:true
            }
        };
        checkMySystem();
    });