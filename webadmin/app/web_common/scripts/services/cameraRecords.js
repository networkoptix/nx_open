'use strict';

angular.module('nxCommon')
    .factory('cameraRecords', ['$q', function ($q) {
        var lastRecordProvider = null;
        var lastPositionProvider = null;
        return {
            getRecordsProvider:function(cameras,mediaserver,width,timeLatency){
                if(lastRecordProvider){
                    lastRecordProvider.abort("getRecordsProvider");
                }
                lastRecordProvider = new CameraRecordsProvider(cameras,mediaserver,$q,width,timeLatency);
                return lastRecordProvider;
            },
            getPositionProvider:function(cameras,mediaserver,timeLatency){
                if(lastPositionProvider){
                    lastPositionProvider.abort("getPositionProvider");
                }
                lastPositionProvider = new ShortCache(cameras,mediaserver,$q,timeLatency);
                return lastPositionProvider;
            }
        };
    }]);