'use strict';

angular.module('webadminApp')
    .factory('cameraRecords', function (mediaserver,$q) {
        return {
            getRecordsProvider:function(cameras,width,timeCorrection){
                return new CameraRecordsProvider(cameras,mediaserver,$q,width,timeCorrection);
            },
            getPositionProvider:function(cameras,timeCorrection){
                return new ShortCache(cameras,mediaserver,$q,timeCorrection);
            }
        };
    });