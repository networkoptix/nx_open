'use strict';

angular.module('cloudApp')
    .controller('DebugCtrl', ['$scope', 'cloudApi', 'account', 'process', '$q', '$timeout',
                'dialogs', 'urlProtocol', '$base64', 'systemsProvider',
        function ($scope, cloudApi, account, process, $q, $timeout,
                  dialogs, urlProtocol, $base64, systemsProvider) {

        account.requireLogin();

        $scope.user_email = 'ebalashov@networkoptix.com';
        $scope.type = 'activate_account';
        $scope.message = JSON.stringify({code:'test_code'}, null, '\t');

        $scope.formatJSON = function(data){
            return  JSON.stringify(data, null, '\t');
        };

        $scope.debugProcess = {
            success: true,
            process: process.init(function () {
                var deferred = $q.defer();
                $timeout(function () {
                    if ($scope.debugProcess.success) {
                        deferred.resolve({
                            data:{
                                resultCode: L.errorCodes.ok
                            }
                        });
                    } else {
                        deferred.reject(false);
                    }
                }, 2000);
                return deferred.promise;
            },{
                successMessage:'Success!',
                errorPrefix:'Fail!'
            })
        };
        var notifyCounter = 0;
        $scope.notify = function(){
            var states = ['warning','info','success','danger'];
            var type = states[Math.floor(Math.random() * states.length)];
            var hold = Math.random() > 0.9;
            dialogs.notify((notifyCounter++) + ':' + type + ': ' + hold, type, hold);
        };
        $scope.testNotification = function(){
            $scope.result = null;
            var message = $scope.message;
            try {
                message = JSON.parse(message);
            }catch(a){
                $scope.result = 'message is not a valud JSON object';
                console.warn ('message is not json', message);
            }
            cloudApi.notification_send($scope.user_email,$scope.type,message).
                then(function(a){
                        $scope.notificationError = false;
                        $scope.result = $scope.formatJSON(a.data);
                        console.log(a);
                    },
                    function(a){
                        $scope.notificationError = true;
                        $scope.result = a.data.errorText;
                        console.error(a);
                    }
                );
        }


        $scope.linkSettings = {
            native: true,
            from: null,    // client, mobile, portal, webadmin
            context: null,
            command: null, // client, cloud, system
            systemId: null,
            action: null,
            actionParameters: null, // Object with parameters
            auth: null // true for request, null for skipping, string for specific value
        }
        $scope.actionParameters = '{\n	"example": true\n}';
        $scope.actionParametersError = false;
        function clearEmptyStrings(obj){
            var ret = $.extend({},obj);
            for( var key in obj){
                if(ret[key] === '' || ret[key] === null){
                    delete ret[key];
                }
            }
            return ret;
        }
        $scope.generateLink = function(){
            $scope.linkSettings.actionParameters = null;
            try{
                $scope.actionParametersError = false;
                if($scope.actionParameters  && $scope.actionParameters != ''){
                    $scope.linkSettings.actionParameters = JSON.parse($scope.actionParameters);
                }
            }catch (a){
                $scope.actionParametersError = true
            }
            return urlProtocol.generateLink(clearEmptyStrings($scope.linkSettings));
        }
        $scope.openLink = function(){
            $scope.linkSettings.actionParameters = null;
            try{
                $scope.actionParametersError = false;
                if($scope.actionParameters  && $scope.actionParameters != ''){
                    $scope.linkSettings.actionParameters = JSON.parse($scope.actionParameters);
                }
            }catch (a){
                $scope.actionParametersError = true
            }

            urlProtocol.getLink(clearEmptyStrings($scope.linkSettings)).then(function(data){
                var link = data.link;
                window.protocolCheck(link, function () {
                    alert('protocol not recognized');
                },function () {
                    alert('ok - procotol is working');
                });
            });
        }

        $scope.getTempKey = function(){
            account.authKey().then(function(authKey){
                $scope.linkSettings.auth = authKey;
            },function(no_account){
                console.error('couldn\'t retrieve temporary auth_key from cloud_portal',no_account);
                $scope.linkSettings.auth = 'couldn\'t retrieve temporary auth_key from cloud_portal';
            });
        }



        $scope.systemsProvider = systemsProvider;
        $scope.$watch('systemsProvider.systems', function(){
            $scope.systems = $scope.systemsProvider.systems;
        });
        $scope.mergeSettings={
            masterSystemId:null,
            slaveSystemId:null
        }
        $scope.mergeSystems = function(){
            $scope.mergeSettings.result = 'working';
            cloudApi.merge($scope.mergeSettings.masterSystemId,$scope.mergeSettings.slaveSystemId).then(function(success){
                $scope.mergeSettings.result = JSON.stringify(success, null, 2);
            },function(error){
                $scope.mergeSettings.result = JSON.stringify(error, null, 2);
            });
        }


    }]);