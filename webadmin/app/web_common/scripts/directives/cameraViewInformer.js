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
        		scope.cameraStates = L.cameraStates;
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
                    
                    //if non flag break
                    if(!scope.alertType){
                        return;
                    }

                    //offline and unauthorized are special cases. All others can be set without editing
                    if(scope.alertType == 'status' && scope.flags[scope.alertType] != "Online"){
                        scope.message = scope.cameraStates[(scope.flags[scope.alertType]).toLowerCase()];
                    }
                    else{
                        scope.message = scope.cameraStates[scope.alertType];
                    }
                }, true);
        	}
		};
	});