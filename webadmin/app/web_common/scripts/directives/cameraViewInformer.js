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


        		scope.$watch('viewFlags', function(){
        			if((scope.viewFlags.status == "Offline" || scope.viewFlags.status == 'Unauthorized') &&
        			   !scope.viewFlags.positionSelected || scope.viewFlags.iosVideoTooLarge)
        			{
        				scope.showView = true;
        			}
        			else{
        				scope.showView = false;
        			}
        		}, true);
        		
        		scope.$watch('videoFlags', function(){
        			scope.showVideo = _.find(scope.videoFlags, function(flag){
											if(flag)
												return true;
										});
        		}, true);
        	}
		};
	});