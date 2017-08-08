'use strict';

angular.module('webadminApp')
    .controller('ApiToolCtrl', function ($scope, mediaserver, $sessionStorage, $location, dialogs, $timeout, systemAPI) {

        $scope.Config = Config;
        $scope.session = $sessionStorage;
        $scope.allowProprietary = true;

        mediaserver.getApiMethods().then(function(data){
            $scope.apiGroups = data.data.groups;
        });

        $scope.setActiveFunction = function(group, method){
            $scope.activeGroup = group;
            $scope.activeFunction = method;
            $scope.session.method.method = method.method;
            $scope.session.method.name = group.urlPrefix + '/' + method.name;
        }
        $scope.clearActiveFunction = function(){
            $scope.activeGroup = null;
            $scope.activeFunction = null;
        }

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
                $scope.result.result = JSON.stringify(success.data, null,  '  ');
            },function(error){
                $scope.result.status = error.status +  ':  ' + error.statusText;
                $scope.result.result =   'Error: ' + JSON.stringify(error.data, null,  '  ');
            });
        };

    });