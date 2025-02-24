// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict"

var elements = {
    remoteVideo: null,
    muteButton: null,
    streamButton: null,
    pauseButton: null,
    nextFrameButton: null,
    seekInput: null,
    timestamp: null,
    transcoding: null,
    hiddenName: "hidden"};
// ws connection
var serverConnection;
var webrtc = {
    peerConnection: null,
    remoteDataChannel: null,
    streamValue: null,
    pauseValue: false,
    lastTimestampMs: null,
    deliveryMethod: null};
var mse = {
    mediaSource: null,
    sourceBuffer: null,
    buffersArray: null,
    mimeType: null};

var peerConnectionConfig = {
  'iceServers': []
};

function getHiddenName() {
  if (typeof document.hidden !== "undefined") {
    elements.hiddenName = "hidden";
  } else if (typeof document.msHidden !== "undefined") {
    elements.hiddenName = "msHidden";
  } else if (typeof document.webkitHidden !== "undefined") {
    elements.hiddenName = "webkitHidden";
  }
}

function initWebRtc(config) {
  elements.remoteVideo = document.getElementById('remoteVideo');
  elements.muteButton = document.getElementById('muteButton');
  elements.streamButton = document.getElementById('streamButton');
  elements.pauseButton = document.getElementById('pauseButton');
  elements.nextFrameButton = document.getElementById('nextFrameButton');
  elements.timestamp = document.getElementById('timestamp');
  elements.transcoding = document.getElementById('transcoding');
  elements.seekInput = document.getElementById('seekInput');

  getHiddenName();

  const queryString = window.location.search;
  const urlParams = new URLSearchParams(queryString);
  webrtc.streamValue = urlParams.get('stream');
  if(webrtc.streamValue != '0' && webrtc.streamValue != '1')
    webrtc.streamValue = '0';
  if(webrtc.streamValue == '0') {
    elements.streamButton.textContent = "High";
  } else {
    elements.streamButton.textContent = "Low";
  }

  webrtc.lastTimestampMs = urlParams.get('position');
  reconnect(urlParams.get('position'), config);
}

function reconnect(position, config) {
  elements.pauseButton.textContent = "||";
  webrtc.pauseValue = false;
  webrtc.peerConnection = null;
  mse.buffersArray = new Array();
  const queryString = window.location.search;
  const urlParams = new URLSearchParams(queryString);
  const apiVersion = urlParams.get('api');
  const speed = urlParams.get('speed');
  webrtc.deliveryMethod = (urlParams.get('deliveryMethod') == 'mse') ? 'mse' : 'srtp';
  if (webrtc.deliveryMethod == 'srtp') {
    elements.pauseButton.disabled = false;
  }

  function post(path, data = {}) {
    return fetch('https://' + config.vmsHost + ':' + config.vmsPort + path,
      {
        method: 'POST',
        headers:
        {
          'Content-Type': 'application/json',
          "Authorization": "Bearer " + config.adminSessionToken
        },
        body: JSON.stringify(data)
      })
      .then(r => r.json())
  }

  if (apiVersion == null || apiVersion != 'legacy')
  {
    const resolution = urlParams.get('resolution');
    const resolutionWhenTranscoding = urlParams.get('resolutionWhenTranscoding');
    const rotation = urlParams.get('rotation');
    const aspectRatio = urlParams.get('aspectRatio');
    const dewarping = urlParams.get('dewarping');
    const dewarpingXangle = urlParams.get('dewarpingXangle');
    const dewarpingYangle = urlParams.get('dewarpingYangle');
    const dewarpingFov = urlParams.get('dewarpingFov');
    const dewarpingPanofactor = urlParams.get('dewarpingPanofactor');
    const zoom = urlParams.get('zoom');
    const panoramic = urlParams.get('panoramic');

    function initWebSocket(url) {
      serverConnection = new WebSocket(url);

      serverConnection.onopen = () => {
        console.log('WebSocket connection established');
      };

      serverConnection.onerror = (error) => {
        console.error('WebSocket error:', error);
      };

      serverConnection.onclose = () => {
        console.log('WebSocket connection closed');
      };

      serverConnection.onmessage = gotMessageFromServer;
    }

    post('/rest/v4/login/tickets', {})
      .then(r => {
        console.log('Got token: ' + r.token);
        let url = 'wss://'
          + config.vmsHost
          + ':'
          + config.vmsPort
          + '/rest/v3/devices/'
          + config.cameraId
          + '/webrtc'
          + '?_ticket=' + r.token
          + '&stream='
          + webrtc.streamValue
          + (speed != null ? '&speed=' + speed : '')
          + '&deliveryMethod=' + webrtc.deliveryMethod
          + (position != null ? ('&positionMs=' + parseInt(position)) : '')
          + (resolutionWhenTranscoding != null ? ('&resolutionWhenTranscoding=' + resolutionWhenTranscoding) : '')
          + (resolution != null ? ('&resolution=' + resolution) : '')
          + (rotation != null ? ('&rotation=' + rotation) : '')
          + (aspectRatio != null ? ('&aspectRatio=' + aspectRatio) : '')
          + (dewarping != null ? ('&dewarping=' + dewarping) : '')
          + (dewarpingXangle != null ? ('&dewarpingXangle=' + dewarpingXangle) : '')
          + (dewarpingYangle != null ? ('&dewarpingYangle=' + dewarpingYangle) : '')
          + (dewarpingFov != null ? ('&dewarpingFov=' + dewarpingFov) : '')
          + (dewarpingPanofactor != null ? ('&dewarpingPanofactor=' + dewarpingPanofactor) : '')
          + (zoom != null ? ('&zoom=' + zoom) : '')
          + (panoramic != null ? ('&panoramic=' + panoramic) : '');

        console.log('Url: ' + url);

        initWebSocket(url);
    })
    .catch(e => console.log('error', 'Login error', e))
  }
  else
  {
    post('/rest/v4/login/tickets', {})
      .then(r => {
        let url = 'wss://'
          + config.vmsHost
          + ':'
          + config.vmsPort
          + '/webrtc-tracker/'
          + '?_ticket=' + r.token
          + '&camera_id='+ config.cameraId
          + (position != null ? ('&position=' + position) : '')
          + '&speed=' + urlParams.get('speed')
          + '&stream=' + webrtc.streamValue
          + '&access_token=' + config.adminSessionToken;

        console.log('Url: ' + url);

        initWebSocket(url);
      })
      .catch(e => console.log('error', 'Login error', e))
  }
}

function restartMse() {
  if (mse.sourceBuffer != null) {
    mse.sourceBuffer.abort();
    mse.sourceBuffer = null;
  }
  mse.mediaSource = null;
  startMse();
}

function startMse() {
  if(!window.MediaSource) {
    console.error('No Media Source API available');
    return;
  }
  mse.mediaSource = new MediaSource();
  elements.remoteVideo.src = window.URL.createObjectURL(mse.mediaSource);
  mse.mediaSource.onsourceopen = () => {
    console.log('ms is opened');
    if (mse.mimeType)
      startSourceBuffer();
  };
}

function appendToBuffer()
{
  if (mse.sourceBuffer && mse.buffersArray.length > 0) {
    console.log('ms add from buffers');
    var buffer = mse.buffersArray.shift();
    mse.sourceBuffer.appendBuffer(buffer);
  }
}

function updateMseAudio()
{
  if(mse.sourceBuffer != null
    && mse.mimeType.indexOf('mp4a.') != -1) { // Check for compatible audio codec.
    muteUpdate();
  } else {
    elements.muteButton.textContent = "No audio";
    elements.muteButton.disabled = true;
  }
}

function startSourceBuffer()
{
  if (mse.sourceBuffer != null)
    return;
  if (!MediaSource.isTypeSupported(mse.mimeType)) {
    console.log('mime is not supported: ' + mse.mimeType);
    return;
  }
  console.log('start source buffer');
  mse.sourceBuffer = mse.mediaSource.addSourceBuffer(mse.mimeType);
  mse.sourceBuffer.mode = "sequence";

  updateMseAudio();
  appendToBuffer();
  mse.sourceBuffer.onupdateend = () => {
    console.log('ms update end');
    appendToBuffer();
  };
  mse.sourceBuffer.onerror = event => {
    console.log('ms update error ' + event);
    reconnectHandler(event);
  };

  // FIXME: is not correct way to start play, see https://goo.gl/LdLk22
  var playPromise = elements.remoteVideo.play();
  if (playPromise != undefined) {
    playPromise.then(_ => {
      console.log('play started');
    })
    .catch(error => {
      console.log('error with play started: ' + error);
      reconnectHandler(error);
    });
  }
}

function start() {
  webrtc.peerConnection = new RTCPeerConnection(peerConnectionConfig);
  webrtc.peerConnection.onicecandidate = gotIceCandidate;
  webrtc.peerConnection.ontrack = gotRemoteStream;
  webrtc.peerConnection.oniceconnectionstatechange = () => {
    console.log('peerConnection ice state ' + webrtc.peerConnection.iceConnectionState)
  };

  webrtc.peerConnection.addEventListener('datachannel', event => {
    console.log('got remote data channel');
    webrtc.remoteDataChannel = event.channel;
    webrtc.remoteDataChannel.binaryType = 'arraybuffer';
    webrtc.remoteDataChannel.addEventListener('message', event => {
      if (typeof(event.data) === 'string') {
        var message = JSON.parse(event.data);
        var timestampMs = parseInt(message.timestampMs);
        webrtc.lastTimestampMs = timestampMs;
        var currentTimestampMs = Math.floor(Date.now());
        var diff = currentTimestampMs - timestampMs;
        console.log('dc message: ' + event.data + ' timestampMs diff: ' + diff);
        elements.timestamp.textContent = event.data;
        // Possible status:
        // 100 (before seek),
        // 200 (after successful seek or stream change)
        // 301 (reconnect),
        // 404 (after seek, data for this timestamp is not found)
        if (message.status && message.status == 301) {
          reconnect(timestampMs);
        } else if (message.status && message.status == 100) {
          if (webrtc.deliveryMethod != null && webrtc.deliveryMethod == 'mse') {
            // Note that initial segment can be received before 200, so restarting MSE on 100.
            restartMse();
          }
        }
      } else {
        var buffer = new Uint8Array(event.data);
        console.log('dc binary: type = ' + typeof(event.data) +  ' len = ' + buffer.length);
        var buffer = new Uint8Array(event.data);
        if (mse.sourceBuffer == null || mse.sourceBuffer.updating) {
          mse.buffersArray.push(buffer);
          console.log('ms pushed to buffer array, length = ' + mse.buffersArray.length);
        } else if (!document[elements.hiddenName]) {
          try {
            mse.sourceBuffer.appendBuffer(buffer);
          } catch(error) {
            reconnectHandler();
          }
        } else {
            console.log("skip buffer on hidden page");
        }
      }
    });
  });
}

function dataClickSeek() {
  webrtc.remoteDataChannel.send(JSON.stringify({'seek': elements.seekInput.value}));
}

function pauseToggle() {
  if (webrtc.pauseValue) {
    webrtc.remoteDataChannel.send(JSON.stringify({'resume': true}));
    elements.pauseButton.textContent = "||";
    elements.nextFrameButton.disabled = true;
  } else {
    webrtc.remoteDataChannel.send(JSON.stringify({'pause': true}));
    elements.pauseButton.textContent = ">";
    elements.nextFrameButton.disabled = false;
  }
  webrtc.pauseValue = !webrtc.pauseValue;
}

function nextFrame() {
  webrtc.remoteDataChannel.send(JSON.stringify({'nextFrame': true}));
}

function streamToggle() {
  if(webrtc.streamValue == '1') {
    webrtc.streamValue = '0';
    elements.streamButton.textContent = "High";
  } else {
    webrtc.streamValue = '1';
    elements.streamButton.textContent = "Low";
  }
  console.log('send stream: ' + webrtc.streamValue);
  webrtc.remoteDataChannel.send(JSON.stringify({'stream': webrtc.streamValue}));
}

function gotMessageFromServer(message) {
  if(!webrtc.peerConnection) start();

  var signal = JSON.parse(message.data);

  if(signal.mime) {
    mse.mimeType = signal.mime;
    if(mse.mediaSource != null && mse.mediaSource.readyState == 'open')
      startSourceBuffer();
  }
  if(signal.transcoding != null) {
    elements.transcoding.textContent = "transcoding: ";
    if(signal.transcoding.video) {
      elements.transcoding.textContent += "video";
      if(signal.transcoding.audio) {
        elements.transcoding.textContent += ", ";
      }
    }
    if(signal.transcoding.audio)
      elements.transcoding.textContent += "audio";
    if(!signal.transcoding.video && !signal.transcoding.audio)
      elements.transcoding.textContent += "none";
  }
  if(signal.sdp) {
    webrtc.peerConnection.setRemoteDescription(new RTCSessionDescription(signal.sdp)).then(function() {
      // Only create answers in response to offers
      if(signal.sdp.type == 'offer') {
        webrtc.peerConnection.createAnswer().then(createdDescription).catch(errorHandler);
      }
    }).catch(errorHandler);
  } else if(signal.ice) {
    webrtc.peerConnection.addIceCandidate(new RTCIceCandidate(signal.ice)).catch(reconnectHandler);
  }
}

function gotIceCandidate(event) {
  if(event.candidate != null) {
    serverConnection.send(JSON.stringify({'ice': event.candidate}));
  }
}

function createdDescription(description) {
  console.log('got description');

  webrtc.peerConnection.setLocalDescription(description).then(function() {
    serverConnection.send(JSON.stringify({'sdp': webrtc.peerConnection.localDescription}));
  }).catch(reconnectHandler);
}

function gotRemoteStream(event) {
  console.log('got remote stream');
  elements.remoteVideo.srcObject = event.streams[0];
  if(event.streams[0].getAudioTracks().length > 0) {
    muteUpdate();
  } else {
    elements.muteButton.textContent = "No audio";
    elements.muteButton.disabled = true;
  }
}

// https://developer.chrome.com/blog/autoplay/
function muteToggle() {
  elements.remoteVideo.muted = !elements.remoteVideo.muted;
  muteUpdate();
}

function muteUpdate() {
  elements.muteButton.disabled = false;
  if (elements.remoteVideo.muted) {
    elements.muteButton.textContent = "Unmute";
  } else {
    elements.muteButton.textContent = "Mute";
  }
}

function errorHandler(error) {
  console.log(error);
  start();
  serverConnection.send(JSON.stringify({'error': error}));
}

function reconnectHandler(error) {
  console.log(error);
  reconnect(webrtc.lastTimestampMs);
}

const clearCanvas = (context) => {
    context.clearRect(0, 0, context.canvas.width, context.canvas.height);
};

const translateToLogicalCoordinateSystem = (context, rect) => {
    const width = context.canvas.width;
    const height = context.canvas.height;

    const scaled = {};
    scaled.x = rect.x / width;
    scaled.y = rect.y / height;
    scaled.width = rect.width / width;
    scaled.height = rect.height / height;

    console.log("Scaled:", scaled);

    return scaled;
};

const drawObject = (context, rect) => {
    context.strokeRect(rect.x, rect.y, rect.width, rect.height);
};

const isPointInRect = (point, rect) => {
    return point.x > rect.x && point.x < rect.x + rect.width
        && point.y > rect.y && point.y < rect.y + rect.height;
};

const appContext = {
    object: {
        x: 100,
        y: 100,
        width: 100,
        height: 100
    },
    movementContext: null,
    canvasContext: null
};

const onMousedown = (event) => {
    const point = {x: event.offsetX, y: event.offsetY};
    if (!isPointInRect(point, appContext.object))
        return;

    appContext.movementContext = {
        capturePointOffsetX: event.offsetX - appContext.object.x,
        capturePointOffsetY: event.offsetY - appContext.object.y
    };
};

const onMouseup = (event) => {
    appContext.movementContext = null;
};

const onMousemove = (event) => {
    if (!appContext.movementContext)
        return;

    clearCanvas(appContext.canvasContext);
    appContext.object.x = event.offsetX - appContext.movementContext.capturePointOffsetX;
    appContext.object.y = event.offsetY - appContext.movementContext.capturePointOffsetY;
    drawObject(appContext.canvasContext, appContext.object);
    window.mainProcess.ipc.send(
        "object-move",
        translateToLogicalCoordinateSystem(appContext.canvasContext, appContext.object))
};

const renderState = (state) => {
    const connectionButton = document.getElementById("connection-button");
    const connectionStatusDisplay = document.getElementById("connection-status");

    connectionButton.textContent = state.connectionState == "connected"
        ? "Disconnect RPC client(as an Integration)"
        : "Connect RPC client(as an Integration)";


    connectionStatusDisplay.textContent = state.connectionState == "connected"
        ? "Connected"
        : "Disconnected";
}

const initCanvas = () => {
    const canvas = document.getElementById("video-canvas");
    canvas.width = canvas.clientWidth;
    canvas.height = canvas.clientHeight;
    canvas.addEventListener("mousedown", onMousedown);
    canvas.addEventListener("mousemove", onMousemove);
    canvas.addEventListener("mouseup", onMouseup);

    appContext.canvasContext = canvas.getContext("2d");
    drawObject(appContext.canvasContext, appContext.object);
    window.mainProcess.ipc.send(
        "object-move",
        translateToLogicalCoordinateSystem(appContext.canvasContext, appContext.object)
    );
}

const initControls = () => {
    const connectButton = document.getElementById("connection-button");
    connectButton.addEventListener("click", () => {
        window.mainProcess.ipc.send("connect-disconnect");
    });

    const makeIntegrationRequestButton = document.getElementById("make-integration-request-button");
    makeIntegrationRequestButton.addEventListener("click", () => {
        window.mainProcess.ipc.send("make-integration-request");
    });

    const approveIntegrationRequestButton =
        document.getElementById("approve-integration-request-button");
    approveIntegrationRequestButton.addEventListener("click", () => {
        window.mainProcess.ipc.send("approve-integration-request");
    });

    const removeIntegrationButton = document.getElementById("remove-integration-button");
    removeIntegrationButton.addEventListener("click", () => {
        window.mainProcess.ipc.send("remove-integration");
    });
}

const initNotifications = () => {
    window.mainProcess.ipc.on("state-changed", (event, data) => {
        console.log("State changed, re-rendering state");
        renderState(data);
    });

    window.mainProcess.ipc.on('initialize-webrtc', (event, config) => {
        console.log("Frontend: Initializing WebRTC, config:", config);
        initWebRtc(config);
    });
}

const onReady = () => {
    initNotifications();
    initCanvas();
    initControls();
};

document.addEventListener("DOMContentLoaded", onReady);
