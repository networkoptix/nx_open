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

        this.initControls = function(cameraDetails, switchPlaying, switchPosition){
            this.cameraDetails = cameraDetails;
            this.switchPlaying = switchPlaying;
            this.switchPosition = switchPosition;
        };

        recognition.onresult = function(event) {
            var last = event.results.length - 1;
            var text = event.results[last][0].transcript.replace(/^\s+|\s+$/g, '');
            console.log(text);
            console.log('Confidence: ' + event.results[0][0].confidence);
            switch(text){
                case "play":
                case "continue":
                    self.switchPlaying(true);
                    break;
                case "pause":
                case "stop":
                    self.switchPlaying(false);
                    break;
                case "live":
                case "go to live":
                    self.switchPosition();
                    break;
                case "open details":
                    self.cameraDetails.enabled = true;
                    break;
                case "close details":
                    self.cameraDetails.enabled = false;
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