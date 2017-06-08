'use strict';

angular.module('nxCommon')
	.directive('cameraViewInformer',function(){
		return{
			restrict: 'E',
        	scope:{
        	    flags: "=",
        	    preloader: "="
        	},
        	templateUrl: Config.viewsDirCommon + 'components/placeholder.html',
        	link: function(scope){
                scope.$watch('flags', function(){
                    scope.title = null;
                    scope.message = null;
                    scope.iconClass = null;
                    scope.condition = false;
                    scope.alertType = null;

                    //check for flag
                    for(var key in scope.flags){
                        if(scope.flags[key]){
                            scope.alertType = key;
                            break;
                        }
                    }
                    
                    //if status is online dont show any message
                    if(scope.alertType == 'status' && (scope.flags[scope.alertType] == "Online" || scope.flags[scope.alertType] == "Recording"))
                    {
                        scope.alertType = null;
                    }
                    //if non flag break
                    if(!scope.alertType){
                        return;
                    }

        	        scope.condition = true;

                    //offline and unauthorized are special cases. All others can be set without editing
                    if(scope.alertType == 'status'){
                        scope.title = L.common.cameraStates[(scope.flags[scope.alertType]).toLowerCase()];
                        scope.message = null;
                        scope.iconClass = scope.flags[scope.alertType] == 'Offline' ? 'camera-view-offline' : 'camera-view-unauthorized';
                    }
                    else{
                        scope.iconClass = 'camera-view-error';
                        scope.title = L.common.cameraStates.error;
                        scope.message = L.common.cameraStates[scope.alertType];
                    }
                }, true);
        	}
		};
	});