'use strict';

angular.module('webadminApp')
    .controller('DevtoolsCtrl', ['$scope', 'mediaserver', '$sessionStorage',
                                 '$location', 'dialogs', '$timeout', 'systemAPI',
    function ($scope, mediaserver, $sessionStorage,
              $location, dialogs, $timeout, systemAPI) {

        $scope.Config = Config;
        $scope.session = $sessionStorage;


        if($scope.Config.developersFeedbackForm){
            $scope.developersFeedbackForm =
                $scope.Config.developersFeedbackForm.replace("{{PRODUCT}}",encodeURIComponent(Config.productName));
        }

        /* API Test Tool */
        if(!$scope.session.method){
            $scope.session.method = {
                name:'/api/moduleInformation',
                data:'',
                params: '',
                method:'GET'
            };
        }

        $scope.result={

        };

        $scope.getDebugUrl = function(){
            var params = $scope.session.method.params.trim();
            if(params && params.indexOf('?')!==0 && $scope.session.method.name.indexOf('?')<0){
                params = '?' + params;
            }

            return mediaserver.debugFunctionUrl($scope.session.method.name + params);
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


        /*  Generic Events Tool */
        $scope.session.event = null;
        if(!$scope.session.event){
            $scope.session.event = {
                state: null
            };
        }
        $scope.generateEventLink = function(){
            var event = _.clone($scope.session.event);
            if(event.state == null){
                delete event.state;
            }
            return mediaserver.debugFunctionUrl('/api/createEvent', event);
        }
        $scope.generateEvent = function(){

            var event = _.clone($scope.session.event);
            if(event.state == null){
                delete event.state;
            }

            if(event.metadata){
                try {
                    $scope.session.event.metadata = JSON.stringify(JSON.parse(event.metadata));
                }catch(a){
                    dialogs.alert('metadata is not a valid json object ');
                    return;
                }
            }
            return mediaserver.createEvent(event).then(function(success){
                $scope.result.status = success.status + ':  ' + success.statusText;
                $scope.result.result = JSON.stringify(success.data, null,  '\t ');
            },function(error){
                $scope.result.status = error.status + ':  ' + error.statusText;
                $scope.result.result = 'Error: ' + JSON.stringify(error.data, null,  '\t ');
            });
        };



    }]);
