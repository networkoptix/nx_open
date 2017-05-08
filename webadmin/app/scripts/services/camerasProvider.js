'use strict';

angular.module('webadminApp')
    .factory('camerasProvider', function () {
    	var cameras = {};

    	return{
    		setCameras: function(cams){
    			cameras = cams;
    		},
    		getCamera: function(id){
                if(!cameras) {
                    return null;
                }
                for(var serverId in cameras) {
                    var cam = _.find(cameras[serverId], function (camera) {
                        return camera.id === id || camera.physicalId === id;
                    });
                    if(cam){
                        return cam;
                    }
                }
                return null;
            }
    	}

    });