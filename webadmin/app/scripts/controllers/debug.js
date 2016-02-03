'use strict';

angular.module('webadminApp')
    .controller('DebugCtrl', function ($scope, mediaserver) {

        mediaserver.getUser().then(function(user){
            if(!user.isOwner){
                $location.path('/info'); //no admin rights - redirect
            }
        });

        // '/api/pingSystem?password=' + password  + '&url=' + encodeURIComponent(url));
        $scope.method={
            name:'/api/pingSystem',
            data:'',
            params:'{\n\t"password":"admin",\n\t"url":"http://demo.networkoptix.com:7001/"\n}',
            method:'POST'
        };

        $scope.getDebugUrl = function(){

            var params = $scope.method.params;
            if(params && params!='') {
                try {
                    params = JSON.parse(params);
                } catch (a) {
                    return "GET-params is not a valid json object";
                }
            }

            return mediaserver.debugFunctionUrl($scope.method.name, params);
        };

        $scope.testMethod = function(){
            var data = $scope.method.data;
            if(data && data!='') {
                try {
                    data = JSON.parse(data);
                } catch (a) {
                    console.error(a);
                    alert("POST-params is not a valid json object");
                    return;
                }
            }

            var params = $scope.method.params;
            if(params && params!='') {
                try {
                    params = JSON.parse(params);
                } catch (a) {
                    console.error(a);
                    alert("GET-params is not a valid json object");
                    return;
                }
            }


            mediaserver.debugFunction($scope.method.method, $scope.method.name, params, data).then(function(success){
                console.log(success);
                $scope.method.status = success.status + ": " + success.statusText;
                $scope.method.result = JSON.stringify(success.data, null, "\t");
            },function(error){
                $scope.method.status = error.status + ": " + error.statusText;
                $scope.method.result =  "Error:" + JSON.stringify(error.data, null, "\t");
            });
        }


    });