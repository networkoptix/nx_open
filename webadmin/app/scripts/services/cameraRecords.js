'use strict';

angular.module('webadminApp')
    .factory('cameraRecords', function (mediaserver,$q) {
        return {
            getRecordsProvider:function(cameras,width){
                return new CameraRecordsProvider(cameras,mediaserver,$q,width);
            },
            getPositionProvider:function(cameras){
                return new ShortCache(cameras,mediaserver,$q);
            }
        };
    });