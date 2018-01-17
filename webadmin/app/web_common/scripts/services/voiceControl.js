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
            var text = event.results[last][0].transcript.replace(/^\ /i, '');
            console.log(text);
            console.log('Confidence: ' + event.results[0][0].confidence);
            var voicePatterns = {
                "stop listening": /^(stop|halt|disable)\ (listening|eavsdropping)/i,
                "play": /^(play|continue|start|go on)/im,
                "pause": /^(pause|stop|end|wait)/im,
                "live": /^(live|go to live|jump to live|show me live)/im,
                "open server": /^(open|uncollapse)\ server/im,
                "close server": /^(close|collapse)\ server/im,
                "open all servers": /open (all|servers)/i,
                "close all servers": /close (all|servers)/i,
                "open details": /^(open details|show details|open panel|open camera links)/im,
                "close details": /^(close details|hide details|close panel|hide panel)/im,
                "search": /^(search|look for|find)/im,
                "select": /^(select|choose|use|switch to)\ camera/im,
                "clear search": /^(clear|empty)\ search/im
            };
            var command = null;
            for(var k in voicePatterns){
                if(text.match(voicePatterns[k])){
                    command = k;
                    break;
                }
            }
            text = text.replace(voicePatterns[command], '');
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
                case "open server":
                    var serverName = text.replace(/ /g,'').toLowerCase();
                    self.viewScope.camerasProvider.collapseServer(serverName, false);
                    break;
                case "close server":
                    var serverName = text.replace(/ /g,'').toLowerCase();
                    self.viewScope.camerasProvider.collapseServer(serverName, true);
                    break;
                case "open details":
                    self.cameraDetails.enabled = true;
                    break;
                case "close details":
                    self.cameraDetails.enabled = false;
                    break;
                case "open all servers":
                    self.viewScope.camerasProvider.collapseAllServers(false);
                    break;
                case "close all servers":
                    self.viewScope.camerasProvider.collapseAllServers(true);
                    break;
                case "search":
                    self.viewScope.searchCams = text;
                    break;
                case "clear search":
                    self.viewScope.searchCams = "";
                    break;
                case "select":
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