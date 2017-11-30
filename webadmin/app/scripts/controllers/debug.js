'use strict';

angular.module('webadminApp')
    .controller('DebugCtrl', ['$scope', 'mediaserver', '$sessionStorage', '$location',
                              'dialogs', '$timeout', 'systemAPI',
    function ($scope, mediaserver, $sessionStorage, $location, dialogs, $timeout, systemAPI) {

        mediaserver.getUser().then(function(user){
            if(!user.isOwner){
                $location.path('/info'); //no admin rights - redirect
            }
        });

        $scope.Config = Config;
        $scope.session = $sessionStorage;
        $scope.resources = [];

        systemAPI.getMediaServers().then(function(result){
            $scope.resources =  $scope.resources.concat(result.data);
        });
        systemAPI.getCameras().then(function(result){
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
        };

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
            if(!$scope.session.event.state){
                delete $scope.session.event.state;
            }
            if($scope.session.event.metadata){
                try {
                    $scope.session.event.metadata = JSON.stringify(JSON.parse($scope.session.event.metadata));
                }catch(a){
                    console.error("couldn't decode json, ignoring" , $scope.session.event.metadata);
                }
            }
            return mediaserver.createEvent($scope.session.event).then(function(success){
                var result = "ok";
                var source =  $scope.session.event.event_type == 'UserDefinedEvent'? $scope.session.event.source : $scope.session.eventResource.name;
                if(success.data.error !=="0"){
                    result = success.data.errorString +" (" + success.data.error +") state:" + $scope.session.event.state;
                }
                $scope.eventsLog = $scope.eventsLog || [];
                $scope.eventsLog.push({
                    event: $scope.session.event.event_type,
                    source: source,
                    result: result
                });
            },function(error){
                $scope.eventsLog = $scope.eventsLog || [];
                $scope.eventsLog.push({
                    event: $scope.session.event.event_type,
                    source: source,
                    result: error
                });
            });
        };

        function rand(limit){
            return Math.floor(Math.random()*limit);
        }
        function randomElement(array){
            return array[rand(array.length)];
        }

        var nextCounter = 0;
        function nextElement(array){
            return array[nextCounter++ % array.length];
        }

        function randomEvent(){
            $scope.session.event.event_type = nextElement(Config.debugEvents.events).event;
            $scope.session.eventResource = randomElement($scope.resources);
            $scope.session.event.eventResourceId = $scope.session.eventResource.id;
            $scope.session.event.state = randomElement(Config.debugEvents.states);
            $scope.session.event.reasonCode = randomElement(Config.debugEvents.reasons);
            $scope.session.event.inputPortId = rand(8);
            $scope.session.event.source = "Source " + rand(1000);
            $scope.session.event.caption = "Caption " + rand(1000);
            $scope.session.event.description = "Description " + rand(1000);
        }
        var doRandomEvent = false;
        function eventGenerator(){
            $timeout(function(){
                if(doRandomEvent && !$scope.generatingEvents ||
                    !doRandomEvent && !$scope.repeatingEvents){
                    return;
                }
                if(doRandomEvent) {
                    randomEvent();
                }
                $scope.generateEvent().then(eventGenerator,eventGenerator);
            },1000);
        }

        $scope.generatingEvents = false;
        $scope.startGeneratingEvents = function(){
            $scope.generatingEvents = true;
            doRandomEvent = true;
            eventGenerator(true);
        };
        $scope.stopGeneratingEvents = function(){
            $scope.generatingEvents = false;
            doRandomEvent = false;
        };

        $scope.repeatingEvents = false;
        $scope.startRepeatingEvent = function(){
            $scope.repeatingEvents = true;
            eventGenerator(false);
        };
        $scope.stopRepeatingEvent = function(){
            $scope.repeatingEvents = false;
        };



        $scope.linkSettings = {
            system: 'demo.networkoptix.com:7001',
            getnonce: '/api/moduleInformation',
            physicalId: '00-16-6C-7E-0B-E7',
            cameraIds:[
                '00-1C-A6-01-67-DA',
                'ACTI_E96--A-XX-13K-00850',
                '00-1A-07-0E-EE-6C',
                '00-1C-A6-01-CA-A6',
                'urn_uuid_0b89056d-9de8-e5aa-a50b-ac03207875e4'

             ],
            nonce: '',
            realm:'networkoptix',
            method: 'PLAY',
            login:'demo',
            password:'nxwitness',
            auth:'',
            rtspLink:''
        };
        $scope.formatLink = function(camID){


            $scope.linkSettings.auth = mediaserver.digest($scope.linkSettings.login,
                $scope.linkSettings.password,
                $scope.linkSettings.realm,
                $scope.linkSettings.nonce,
                $scope.linkSettings.method);

            $scope.linkSettings.rtspLink = "rtsp://" + $scope.linkSettings.system + '/' + camID +
                '?auth=' + $scope.linkSettings.auth;
            return $scope.linkSettings.rtspLink;
        };

    }]);