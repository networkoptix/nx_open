'use strict';

angular.module('webadminApp')
    .factory('cameraRecords', function (mediaserver,$q) {
        var lastRecordProvider = null;
        var lastPositionProvider = null;
        return {
            getRecordsProvider:function(cameras,width,timeCorrection){
                if(lastRecordProvider){
                    lastRecordProvider.abort("getRecordsProvider");
                }
                lastRecordProvider = new CameraRecordsProvider(cameras,mediaserver,$q,width,timeCorrection);
                return lastRecordProvider;
            },
            getPositionProvider:function(cameras,timeCorrection){
                if(lastPositionProvider){
                    lastPositionProvider.abort("getPositionProvider");
                }
                lastPositionProvider = new ShortCache(cameras,mediaserver,$q,timeCorrection);
                return lastPositionProvider;
            }
        };
    });