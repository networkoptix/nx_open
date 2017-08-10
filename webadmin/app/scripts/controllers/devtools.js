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
