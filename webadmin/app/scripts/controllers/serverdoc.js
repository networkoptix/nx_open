'use strict';

angular.module('webadminApp')
    .controller('ServerDocCtrl', ['$scope', 'mediaserver',
        function ($scope, mediaserver) {
            
            $scope.Config = Config;
            mediaserver.getServerDocumentation().then(function(data){
                $scope.settings = data.data.reply.settings;
            });
        }]);
