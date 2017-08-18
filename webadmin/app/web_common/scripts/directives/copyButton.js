angular.module('nxCommon')
	.directive('copyButton', ['$timeout', '$log', function ($timeout, $Log) {
		return{
			restrict: 'E',
        	scope:{
                defaultText: "@",
                activeText: "@",
                hoverText: '@',
                copyData: '='
        	},
        	templateUrl: Config.viewsDirCommon + 'components/copyButton.html',
        	link: function(scope){
                
                scope.hoverText = scope.hoverText || L.common.cameraLinks.copyToClipboard;
                scope.defaultText = scope.defaultText || L.common.cameraLinks.copyDefaultText;
                scope.activeText = scope.activeText || L.common.cameraLinks.copyActiveText;
                scope.clipboardSupported = true; // presume
                
                scope.displayedText = scope.defaultText;

                function resetButton(){
        			scope.displayedText = scope.defaultText;
        		}

                var resetDisplayedTextTimer = Config.webclient.resetDisplayedTextTimer;
        		scope.copySuccess = function(){
        			scope.displayedText = scope.activeText;
        			$timeout(resetButton, resetDisplayedTextTimer);
        		};

                scope.copyError = function(err){
                    $log.error('Clipboard error: %s', err);
                };
        	}
		};
	}]);
