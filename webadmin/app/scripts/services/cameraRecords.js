'use strict';

angular.module('webadminApp')
    .factory('cameraRecords', function (mediaserver) {
        return {
            getRecordsProvider:function(cameras){
                return new CameraRecordsProvider(cameras,mediaserver);
            }
        };
    });