'use strict';

angular.module('nxCommon')
    .directive('voiceControl', ['voiceControl', function (voiceControl) {
        return{
        	restrict: 'E',
        	scope:{
        	    'voiceControls': '='
        	},
        	templateUrl: Config.viewsDirCommon + 'components/voiceControl.html',
        	link: function(scope, element){
		        scope.commands = L.common.voiceCommands;
		        scope.pushToTalk = function(){
                    voiceControl.startListening(true);
		        };

		        scope.stopListening = function(){
		            voiceControl.stopListening();
		        };
        	}
        };
    }])
    .service('voiceControl', function () {
        if(jscd.browser.toLowerCase() != 'chrome'){
            return;
        }
        var self = this;
        var SpeechRecognition = SpeechRecognition || webkitSpeechRecognition;
	    var SpeechGrammarList = SpeechGrammarList || webkitSpeechGrammarList;
	    var SpeechRecognitionEvent = SpeechRecognitionEvent || webkitSpeechRecognitionEvent;

	    var recognition = new SpeechRecognition();
	    recognition.continuous = true;
	    recognition.lang = 'en-US';

	    this.startListening = function(pushToTalk) {
	        if(pushToTalk){
	            recognition.continuous = false;
	            self.viewScope.voiceControls.enabled = false;
	        }
	        else{
	            recognition.continuous = true;
	        }
            recognition.start();
            //console.log('Ready to listen.');
        };

        this.stopListening = function(){
            recognition.stop();
            //console.log("Stop listening.");
        };

        this.initControls = function(viewScope){
            this.viewScope = viewScope;
            this.viewScope.$watch('voiceControls.enabled', function(){
                if(self.viewScope.voiceControls.enabled){
                    self.startListening();
                }
                else{
                    self.stopListening();
                }
            });
        };

        recognition.onresult = function(event) {
            var last = event.results.length - 1;
            var text = event.results[last][0].transcript.replace(/^\ /i, '');
            //console.log(text);
            //console.log('Confidence: ' + event.results[0][0].confidence);
            var voicePatterns = {
                "stop listening": /^(stop|halt|disable)\ (listening|eavsdropping)/i,
                "play": /^(play|continue|start|go on)/im,
                "pause": /^(pause|stop|end|wait)/im,
                "live": /^(live|go to live|jump to live|show me live)/im,
                "expand server": /^(open|expand)\ server/im,
                "collapse server": /^(close|collapse)\ server/im,
                "expand all servers": /(open|expand) (all|servers)/i,
                "collapse all servers": /(close|collapse) (all|servers)/i,
                "open camera details": /^(open details|show details|open panel|open camera links)/im,
                "close camera details": /^(close details|hide details|close panel|hide panel)/im,
                "clear search": /^(clear|empty)\ search/im,
                "search": /^(search for|look for|find)/im,
                "view": /^(select|choose|use|switch to|view)/im,
                "help": /^(help|show commands)/im
            };
            var command = null;
            for(var k in voicePatterns){
                if(text.match(voicePatterns[k])){
                    command = k;
                    break;
                }
            }
            self.viewScope.voiceControls.lastCommand = text;
            text = text.replace(voicePatterns[command], '');
            switch(command){
                case "play":
                    self.viewScope.switchPlaying(true);
                    break;
                case "pause":
                    self.viewScope.switchPlaying(false);
                    break;
                case "live":
                    self.viewScope.switchPosition();
                    break;
                case "expand server":
                    var serverName = text.replace(/ /g,'').toLowerCase();
                    self.viewScope.camerasProvider.collapseServer(serverName, false);
                    break;
                case "collapse server":
                    var serverName = text.replace(/ /g,'').toLowerCase();
                    self.viewScope.camerasProvider.collapseServer(serverName, true);
                    break;
                case "open camera details":
                    self.viewScope.cameraLinks.enabled = true;
                    break;
                case "close camera details":
                    self.viewScope.cameraLinks.enabled = false;
                    break;
                case "expand all servers":
                    self.viewScope.camerasProvider.collapseAllServers(false);
                    break;
                case "collapse all servers":
                    self.viewScope.camerasProvider.collapseAllServers(true);
                    break;
                case "search":
                    self.viewScope.searchCams = text;
                    break;
                case "clear search":
                    self.viewScope.searchCams = "";
                    break;
                case "view":
                    var cameraName = text.replace(/ /g,'').toLowerCase();
                    if(cameraName){
                        var camera = self.viewScope.camerasProvider.getFirstAvailableCamera(cameraName);
                        if(camera){
                            self.viewScope.activeCamera = camera;
                        }
                        else{
                            console.log("Camera Not found!!");
                        }
                    }
                    break;
                case "stop listening":
                    self.viewScope.voiceControls.enabled = false;
                    self.stopListening();
                    break;
                case "help":
                    self.viewScope.voiceControls.showCommands = true
                    break;
                default:
                    self.viewScope.voiceControls.lastCommand = "Did not recognize command";
                    console.log("Did not recognize command");
                    break
            }
        };
        recognition.onspeechend = function() {
            recognition.stop();
        };
        recognition.onerror = function(event) {
            self.viewScope.voiceControls.enabled = false;
            console.log('Error occurred in recognition: %s', event.error);
        };
    });