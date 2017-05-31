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
                scope.alertType = null;

        		scope.$watch('viewFlags', function(){
        			if((scope.viewFlags.status == "Offline" || scope.viewFlags.status == 'Unauthorized') &&
        			   !scope.viewFlags.positionSelected || scope.viewFlags.iosVideoTooLarge)
        			{
        				scope.showView = true;
        			}
        			else{
        				scope.showView = false;
        			}

                    scope.alertType = _.find(Object.keys(scope.viewFlags), function(x){
                        if(scope.viewFlags[x])
                            return x;
                    });

                    if(!scope.alertType){
                        return;
                    }
                    
                    if( scope.alertType == 'status'){
                        if(scope.viewFlags[scope.alertType] == "Offline"){
                            scope.message = scope.cameraStates.offline;

                        }
                        else if(scope.viewFlags[scope.alertType] == "Unauthorized"){
                            scope.message = scope.cameraStates.unauthorized;
                        }
                    }
                    else{
                        scope.message = scope.cameraStates[scope.alertType];
                    }
        		}, true);
        		


        		scope.$watch('videoFlags', function(){
                    scope.alertType = _.find(Object.keys(scope.videoFlags), function(x){
                        if(scope.videoFlags[x])
                            return x;
                    });

                    scope.showVideo = scope.alertType != null;

                    if(scope.alertType != "loading")
                        scope.message = scope.cameraStates[scope.alertType];

                    console.log(scope.message);
        		}, true);
        	}
		};
	});