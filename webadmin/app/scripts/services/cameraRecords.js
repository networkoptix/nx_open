'use strict';

angular.module('webadminApp')
    .factory('cameraRecords', function (mediaserver,$q) {
        var lastRecordProvider = null;
        var lastPositionProvider = null;
        return {
            getRecordsProvider:function(cameras,width,timeCorrection){
                if(lastRecordProvider){
                    lastRecordProvider.abort();
                }
                lastRecordProvider = new CameraRecordsProvider(cameras,mediaserver,$q,width,timeCorrection);
                return lastRecordProvider;
            },
            getPositionProvider:function(cameras,timeCorrection){
                if(lastPositionProvider){
                    lastPositionProvider.abort();
                }
                lastPositionProvider = new ShortCache(cameras,mediaserver,$q,timeCorrection);
                return lastPositionProvider;
            }
        };
    });