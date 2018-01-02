'use strict';

angular.module('nxCommon')
    .service('voiceControl', function () {
        if(!window.chrome){
            return;
        }
        var self = this;
        var SpeechRecognition = SpeechRecognition || webkitSpeechRecognition;
	    var SpeechGrammarList = SpeechGrammarList || webkitSpeechGrammarList;
	    var SpeechRecognitionEvent = SpeechRecognitionEvent || webkitSpeechRecognitionEvent;

	    var recognition = new SpeechRecognition();
	    recognition.continuous = true;
	    recognition.lang = 'en-US';
	    this.playerAPI = null;
	    this.cameraLinks = null;

	    this.startListening = function() {
            recognition.start();
            console.log('Ready to listen.');
        };

        this.stopListening = function(){
            recognition.stop();
            console.log("Stop listening.");
        };

        this.initControls = function(cameraDetails, switchPlaying, switchPosition, viewScope){
            this.cameraDetails = cameraDetails;
            this.switchPlaying = switchPlaying;
            this.switchPosition = switchPosition;
            this.viewScope = viewScope;
        };

        recognition.onresult = function(event) {
            var last = event.results.length - 1;
            var text = event.results[last][0].transcript;
            console.log(text);
            console.log('Confidence: ' + event.results[0][0].confidence);
            var voicePatterns = {
                "play": /(play|continue|start)/i,
                "pause": /(pause|stop|end)/i,
                "live": /live/i,
                "open": /open details/i,
                "close": /close details/i,
                "search": /(search|look for|find)/i,
                "select": /(select|choose|use)\ (camera|server)/i
            };
            var command = null;
            for(var k in voicePatterns){
                if(text.match(voicePatterns[k])){
                    command = k;
                    break;
                }
            }
            console.log(command);
            switch(command){
                case "play":
                    self.switchPlaying(true);
                    break;
                case "pause":
                    self.switchPlaying(false);
                    break;
                case "live":
                    self.switchPosition();
                    break;
                case "open":
                    self.cameraDetails.enabled = true;
                    break;
                case "close":
                    self.cameraDetails.enabled = false;
                    break;
                case "search":
                    text = ((text.replace(/^\s+|\s+$/g, '')).replace(voicePatterns["search"], '')).replace(/^\s+|\s+$/g, '');
                    self.viewScope.searchCams = text;
                    break;
                case "select":
                    var server = text.match(/server ([\d]+)/i);
                    var camera = text.match(/camera ([\d]+)/i);
                    console.log(server, camera);
                    if(server && camera){
                        var serverId = self.viewScope.camerasProvider.mediaServers[server[1]].id;
                        //console.log(self.viewScope.camerasProvider.cameras[serverId][camera[1]]);
                        self.viewScope.activeCamera = self.viewScope.camerasProvider.cameras[serverId][camera[1]];
                    }
                    break;
                default:
                    console.log("Did not recognize command");
                    break
            }
        };
        recognition.onspeechend = function() {
            recognition.stop();
        };
        recognition.onerror = function(event) {
            console.log('Error occurred in recognition: %s', event.error);
        };
    });