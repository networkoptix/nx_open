'use strict';

angular.module('webadminApp')
    .controller('DebugCtrl', function ($scope, mediaserver, $sessionStorage, $location, dialogs) {

        mediaserver.getUser().then(function(user){
            if(!user.isOwner){
                $location.path('/info'); //no admin rights - redirect
            }
        });

        $scope.Config = Config;
        $scope.session = $sessionStorage;
        $scope.resources = [];

        mediaserver.getMediaServers().then(function(result){
            $scope.resources =  $scope.resources.concat(result.data);
        });
        mediaserver.getCameras().then(function(result){
            $scope.resources =  $scope.resources.concat(result.data);
        });
        $scope.filterResources = function(filter){
            console.log(filter,!filter || filter === '' || filter === ' ');
            var filtered = _.filter($scope.resources,function(resource){
                return !filter || filter === '' || resource.name.toLowerCase().indexOf(filter.trim().toLowerCase())>=0;
            });
            console.log($scope.resources,filtered);
            return filtered;

        };
        $scope.getId = function(resource){
            return resource.id;
        }

        if(!$scope.session.method){
            $scope.session.method = {
                name:'/api/pingSystem',
                data:'',
                params: JSON.stringify({
                    password: 'admin',
                    url: 'http://demo.networkoptix.com:7001/'
                }, null,  '\t '),
                method:'POST'
            };
        }

        $scope.result={

        };

        $scope.getDebugUrl = function(){

            var params = $scope.session.method.params;
            if(params && params !== '') {
                try {
                    params = JSON.parse(params);
                } catch (a) {
                    return  'GET-params is not a valid json object:  ' + a;
                }
            }

            return mediaserver.debugFunctionUrl($scope.session.method.name, params);
        };

        $scope.testMethod = function(){
            var data = $scope.session.method.data;
            if(data && data !== '') {
                try {
                    data = JSON.parse(data);
                } catch (a) {
                    console.error(a);
                    dialogs.alert( 'POST-params is not a valid json object ');
                    return;
                }
            }

            var params = $scope.session.method.params;
            if(params && params !== '') {
                try {
                    params = JSON.parse(params);
                } catch (a) {
                    console.error(a);
                    dialogs.alert( 'GET-params is not a valid json object ');
                    return;
                }
            }


            mediaserver.debugFunction($scope.session.method.method, $scope.session.method.name, params, data).then(function(success){
                $scope.result.status = success.status +  ':  ' + success.statusText;
                $scope.result.result = JSON.stringify(success.data, null,  '\t ');
            },function(error){
                $scope.result.status = error.status +  ':  ' + error.statusText;
                $scope.result.result =   'Error: ' + JSON.stringify(error.data, null,  '\t ');
            });
        };

        $scope.session.event = {};
        $scope.generateEvent = function(){
            $scope.session.event.eventResourceId = $scope.session.eventResource.id;
            mediaserver.createEvent($scope.session.event).then(function(success){

                $scope.eventsLog = $scope.eventsLog || [];
                $scope.eventsLog.push({
                    event: $scope.session.event.event_type,
                    result: success.data
                });
            },function(error){
                $scope.eventsLog = $scope.eventsLog || [];
                $scope.eventsLog.push({
                    event: $scope.session.event.event_type,
                    result: error
                });
            });
        };
        $scope.generatingEvents = false;
        $scope.startGeneratingEvents = function(){
            $scope.generatingEvents = true;
        };
        $scope.stopGeneratingEvents = function(){
            $scope.generatingEvents = false;
        };


    });