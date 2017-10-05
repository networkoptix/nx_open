'use strict';

angular.module('nxCommon')
	.directive('cameraViewInformer',function(){
		return{
			restrict: 'E',
        	scope:{
        	    flags: "=",
        	    preloader: "=",
                preview: "="
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
                        if(key != 'status' && scope.flags[key]){
                            scope.alertType = key;
                            break;
                        }
                    }
                    //If position has been selected then there is an archive no message is required
                    if(typeof(scope.flags.positionSelected) !== 'undefined' && scope.flags.positionSelected
                                                                            && scope.alertType == "positionSelected")
                    {
                        return;
                    }

                    //if status is online dont show any message
                    if(!scope.alertType && typeof(scope.flags.status) !== 'undefined'
                                        && !(scope.flags.status == "Online" || scope.flags.status == "Recording"))
                    {
                        scope.alertType = 'status';
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
                        scope.iconClass = scope.flags.status == 'Offline' ? 'camera-view-offline' : 'camera-view-unauthorized';
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