'use strict';

angular.module('nxCommon')
	.directive('cameraViewInformer',function(){
		return{
			restrict: 'E',
        	scope:{
        	    flags: "="
        	},
        	templateUrl: Config.viewsDirCommon + 'components/cameraViewInformer.html',
        	link: function(scope){
        		scope.cameraStates = L.common.cameraStates;
                scope.message = "";

                scope.$watch('flags', function(){
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

                    //offline and unauthorized are special cases. All others can be set without editing
                    if(scope.cameraStates){
                        if(scope.alertType == 'status'){
                            scope.message = scope.cameraStates[(scope.flags[scope.alertType]).toLowerCase()];
                        }
                        else{
                            scope.message = scope.cameraStates[scope.alertType];
                        }
                    }
                }, true);
        	}
		};
	});