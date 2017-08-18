angular.module('nxCommon')
	.directive('copyButton', ['$timeout', function ($timeout) {
		return{
			restrict: 'E',
        	scope:{
                clipboardSupported: "=",
                copyDefaultText: "@",
                copyActiveText: "@",
                copyTitle: '@',
                copyData: '='
        	},
        	templateUrl: Config.viewsDirCommon + 'components/copyButton.html',
        	link: function(scope){
                
                scope.copyTitle = scope.copyTitle || L.common.cameraLinks.copyToClipboard;
                scope.copyDefaultText = scope.copyDefaultText || L.common.cameraLinks.copyDefaultText;
                scope.copyActiveText = scope.copyActiveText || L.common.cameraLinks.copyActiveText;
                
                scope.copyText = scope.copyDefaultText;
        		
                function resetButton(){
        			scope.copyText = scope.copyDefaultText;
        		}

                var revertText = Config.webclient.resetCopyText;
        		scope.alertClick = function(){
        			scope.copyText = scope.copyActiveText;
        			$timeout(function(){resetButton();}, revertText);
        		};
        	}
		};
	}]);