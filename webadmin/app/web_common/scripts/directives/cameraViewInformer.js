'use strict';

angular.module('nxCommon')
	.directive('cameraViewInformer',function(){
		return{
			restrict: 'E',
        	scope:{
        		viewFlags: "=",
        		videoFlags: "=",
        		preview: "="
        	},
        	templateUrl: Config.viewsDirCommon + 'components/cameraViewInformer.html',
        	link: function(scope){
        		scope.cameraStates = L.cameraStates;
        		scope.showView = false;
        		scope.showFlags = false;
                scope.message = "";

        		scope.$watch('viewFlags', function(){
                    scope.alertType = null;
                    //if status is offline or unauthorized and no position is selected or ios video is too large show the cameraViewinformer directive
        			if((scope.viewFlags.status == "Offline" || scope.viewFlags.status == 'Unauthorized') &&
        			   !scope.viewFlags.positionSelected || scope.viewFlags.iosVideoTooLarge)
        			{
        				scope.showView = true;
        			}
        			else{
        				scope.showView = false;
        			}


                    //find message to display
                    for(var key in scope.viewFlags){
                        if(scope.viewFlags[key]){
                            scope.alertType = key;
                            break;
                        }
                    }

                    if(!scope.alertType){
                        return;
                    }
                    

                    //If status turn on html and load specific status messages
                    if( scope.alertType == 'status'){
                        if(scope.viewFlags[scope.alertType] == "Offline"){
                            scope.message = scope.cameraStates.offline;
                        }
                        else if(scope.viewFlags[scope.alertType] == "Unauthorized"){
                            scope.message = scope.cameraStates.unauthorized;
                        }
                    }
                    //Otherwise load remaining message
                    else{
                        scope.message = scope.cameraStates[scope.alertType];
                    }
        		}, true);
        		


        		scope.$watch('videoFlags', function(){
                    scope.alertType = null;

                    //find message to display
                    for(var key in scope.videoFlags){
                        if(scope.videoFlags[key]){
                            scope.alertType = key;
                            break;
                        }
                    }
                    scope.showVideo = scope.alertType != null;
                    
                    if(!scope.alertType){
                        return;
                    }

                    //find message to display other than loading
                    if(scope.alertType != "loading"){
                        scope.message = scope.cameraStates[scope.alertType];
                    }
        		}, true);
        	}
		};
	});