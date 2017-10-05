'use strict';

angular.module('nxCommon')
    .factory('cameraRecords', [function () {
        var lastRecordProvider = null;
        var lastPositionProvider = null;
        return {
            getRecordsProvider:function(cameras, mediaserver, width){
                if(lastRecordProvider){
                    lastRecordProvider.abort("getRecordsProvider");
                }
                lastRecordProvider = new CameraRecordsProvider(cameras, mediaserver, width);
                return lastRecordProvider;
            },
            getPositionProvider:function(cameras, mediaserver){
                if(lastPositionProvider){
                    lastPositionProvider.abort("getPositionProvider");
                }
                lastPositionProvider = new ShortCache(cameras, mediaserver);
                return lastPositionProvider;
            }
        };
    }]);