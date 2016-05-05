'use strict';

angular.module('webadminApp')
    .controller('DebugCtrl', function ($scope, mediaserver, $sessionStorage, $location, dialogs) {

        mediaserver.getUser().then(function(user){
            if(!user.isOwner){
                $location.path('/info'); //no admin rights - redirect
            }
        });

        $scope.session = $sessionStorage;

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


    });