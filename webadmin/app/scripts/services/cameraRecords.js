'use strict';

angular.module('webadminApp')
    .factory('cameraRecords', function (mediaserver,$q) {
        return {
            getRecordsProvider:function(cameras){
                return new CameraRecordsProvider(cameras,mediaserver,$q);
            },
            getPositionProvider:function(cameras){
                return new ShortCache(cameras,mediaserver,$q);
            }
        };
    });