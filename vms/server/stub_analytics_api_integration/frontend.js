// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict"

function createBrowserLogger(scope) {
  const prefix = "[" + scope + "]";

  const write = (level, ...args) => {
    const levelPrefix = "[" + level + "]";
    if (!args || args.length === 0) {
      console.log(prefix, levelPrefix);
      return;
    }

    console.log(prefix, levelPrefix, ...args);
  };

  return {
    log: (...args) => write("LOG", ...args),
    info: (...args) => write("INFO", ...args),
    warn: (...args) => write("WARN", ...args),
    error: (...args) => write("ERROR", ...args)
  };
}

let logger = createBrowserLogger("FRONTEND");
if (typeof require === "function") {
  try {
    const { createLogger } = require("./utils.js");
    if (typeof createLogger == "function") {
      logger = createLogger("FRONTEND");
    }
  } catch (error) {
    logger.warn("Failed to load createLogger from utils.js. Using browser logger.", error);
  }
}

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

const kDefaultIceServers = [
  {
    urls: [
      "stun:stun.l.google.com:19302",
      "stun:stun1.l.google.com:19302"
    ]
  },
  {
    urls: [
      "stun:stun2.l.google.com:19302"
    ]
  }
];

function createCandidateTypeCounters() {
  return {
    host: 0,
    srflx: 0,
    relay: 0,
    prflx: 0,
    unknown: 0
  };
}

var webrtc = {
    peerConnection: null,
    peerConnectionAttemptToken: 0,
    remoteDataChannel: null,
    streamValue: null,
    pauseValue: false,
    lastTimestampMs: null,
    deliveryMethod: null,
    activeConfig: null,
    remoteDescriptionSet: false,
    pendingIceCandidates: [],
    attemptId: 0,
    sentIceCandidatesCount: 0,
    receivedIceCandidatesCount: 0,
    remoteDescriptionTimestampMs: null,
    iceConnectivityTimerId: null,
    dataChannelWatchdogTimerId: null,
    remoteDataChannelLastMessageTimestampMs: null,
    localCandidateTypeCounts: createCandidateTypeCounters(),
    remoteCandidateTypeCounts: createCandidateTypeCounters(),
    peerConnectionConfig: null,
    activeAttemptToken: 0,
    activeDeviceId: null,
    attemptPhase: "idle",
    lastInitReason: ""};
var mse = {
    mediaSource: null,
    sourceBuffer: null,
    buffersArray: null,
    mimeType: null};

var peerConnectionConfig = {
  iceServers: kDefaultIceServers,
  iceTransportPolicy: "all"
};

const kIceConnectivityTimeoutMs = 15000;
const kDataChannelInactivityTimeoutMs = 5000;
const kDataChannelWatchdogIntervalMs = 1000;

function normalizeCandidateType(type) {
  if (type == "host" || type == "srflx" || type == "relay" || type == "prflx") {
    return type;
  }

  return "unknown";
}

function incrementCandidateTypeCounter(direction, type) {
  const normalizedType = normalizeCandidateType(type);
  const counters = direction == "local"
    ? webrtc.localCandidateTypeCounts
    : webrtc.remoteCandidateTypeCounts;

  if (!Object.prototype.hasOwnProperty.call(counters, normalizedType)) {
    counters.unknown += 1;
    return;
  }

  counters[normalizedType] += 1;
}

function formatCandidateTypeCounters(counters) {
  return "host=" + counters.host
    + " srflx=" + counters.srflx
    + " relay=" + counters.relay
    + " prflx=" + counters.prflx
    + " unknown=" + counters.unknown;
}

function logCandidateTypeSummary(reason) {
  logger.log(
    "[ICE] " + reason
    + " candidate types local(" + formatCandidateTypeCounters(webrtc.localCandidateTypeCounts) + ")"
    + " remote(" + formatCandidateTypeCounters(webrtc.remoteCandidateTypeCounts) + ")");
}

function isLikelyIpAddress(address) {
  if (!address || address == "unknown") {
    return false;
  }

  if (/^\d{1,3}(?:\.\d{1,3}){3}$/.test(address)) {
    return true;
  }

  return address.indexOf(":") >= 0; //< Treat as IPv6 if contains ":".
}

function isValidIceServerUrl(url) {
  if (typeof url != "string") {
    return false;
  }

  const trimmed = url.trim();
  return trimmed.startsWith("stun:") || trimmed.startsWith("turn:") || trimmed.startsWith("turns:");
}

function normalizeIceServer(entry, index) {
  if (!entry || typeof entry != "object") {
    logger.warn("[ICE] Ignoring invalid iceServers[" + index + "]: entry must be an object");
    return null;
  }

  const rawUrls = Array.isArray(entry.urls) ? entry.urls : [entry.urls];
  const urls = rawUrls
    .filter(url => isValidIceServerUrl(url))
    .map(url => url.trim());

  if (urls.length == 0) {
    logger.warn(
      "[ICE] Ignoring invalid iceServers[" + index + "]: urls must contain stun:/turn:/turns: entries");
    return null;
  }

  const normalized = { urls: urls };

  if (typeof entry.username == "string") {
    normalized.username = entry.username;
  }

  if (typeof entry.credential == "string") {
    normalized.credential = entry.credential;
  }

  if (typeof entry.credentialType == "string") {
    normalized.credentialType = entry.credentialType;
  }

  return normalized;
}

function sanitizeIceServersForLog(iceServers) {
  return iceServers.map(server => ({
    urls: server.urls,
    hasUsername: !!server.username,
    hasCredential: !!server.credential,
    credentialType: server.credentialType || null
  }));
}

function resolvePeerConnectionConfig(config) {
  let configuredIceServers = [];
  if (config && Array.isArray(config.iceServers)) {
    configuredIceServers = config.iceServers
      .map((entry, index) => normalizeIceServer(entry, index))
      .filter(entry => entry != null);
  }

  const effectiveIceServers =
    configuredIceServers.length > 0 ? configuredIceServers : kDefaultIceServers;

  const resolvedConfig = {
    iceServers: effectiveIceServers,
    iceTransportPolicy: "all"
  };

  if (config && (config.iceTransportPolicy == "all" || config.iceTransportPolicy == "relay")) {
    resolvedConfig.iceTransportPolicy = config.iceTransportPolicy;
  } else if (config && config.iceTransportPolicy) {
    logger.warn(
      "[ICE] Unsupported iceTransportPolicy value: " + config.iceTransportPolicy + ". Using 'all'.");
  }

  if (config && Number.isInteger(config.iceCandidatePoolSize)) {
    resolvedConfig.iceCandidatePoolSize = Math.max(0, Math.min(10, config.iceCandidatePoolSize));
  }

  const mode = configuredIceServers.length > 0 ? "config" : "default";
  logger.log(
    "[ICE] Using RTCPeerConnection config"
    + " source=" + mode
    + " transportPolicy=" + resolvedConfig.iceTransportPolicy
    + " poolSize=" + (resolvedConfig.iceCandidatePoolSize || 0)
    + " servers=" + JSON.stringify(sanitizeIceServersForLog(resolvedConfig.iceServers)));

  return resolvedConfig;
}

function parseCandidateInfo(candidate) {
  const empty = {
    type: "unknown",
    protocol: "unknown",
    address: "unknown",
    port: "unknown",
    mid: null,
    mLineIndex: null
  };

  if (!candidate || !candidate.candidate) {
    return empty;
  }

  const tokens = candidate.candidate.trim().split(/\s+/);
  let type = "unknown";
  const protocol = tokens.length > 2 ? tokens[2] : "unknown";
  const address = tokens.length > 4 ? tokens[4] : "unknown";
  const port = tokens.length > 5 ? tokens[5] : "unknown";

  const typTokenIndex = tokens.indexOf("typ");
  if (typTokenIndex >= 0 && typTokenIndex + 1 < tokens.length) {
    type = tokens[typTokenIndex + 1];
  }

  return {
    type: type,
    protocol: protocol,
    address: address,
    port: port,
    mid: candidate.sdpMid,
    mLineIndex: candidate.sdpMLineIndex
  };
}

function logCandidate(direction, candidate) {
  const info = parseCandidateInfo(candidate);
  logger.log(
    '[ICE] '
      + direction
      + ' candidate'
      + ' type=' + info.type
      + ' protocol=' + info.protocol
      + ' address=' + info.address + ':' + info.port
      + ' mid=' + info.mid
      + ' mLine=' + info.mLineIndex);
}

function clearIceConnectivityTimer() {
  if (webrtc.iceConnectivityTimerId != null) {
    clearTimeout(webrtc.iceConnectivityTimerId);
    webrtc.iceConnectivityTimerId = null;
  }
}

function clearDataChannelWatchdog() {
  if (webrtc.dataChannelWatchdogTimerId != null) {
    clearInterval(webrtc.dataChannelWatchdogTimerId);
    webrtc.dataChannelWatchdogTimerId = null;
  }
}

function startDataChannelWatchdog(peerConnection) {
  clearDataChannelWatchdog();

  webrtc.dataChannelWatchdogTimerId = setInterval(() => {
    if (peerConnection !== webrtc.peerConnection) {
      return;
    }

    if (webrtc.peerConnectionAttemptToken !== webrtc.activeAttemptToken) {
      return;
    }

    if (!webrtc.remoteDataChannel || webrtc.remoteDataChannel.readyState !== "open") {
      return;
    }

    const state = peerConnection.iceConnectionState;
    if (state != "connected" && state != "completed") {
      return;
    }

    if (!webrtc.remoteDataChannelLastMessageTimestampMs) {
      return;
    }

    const now = Date.now();
    const gapMs = now - webrtc.remoteDataChannelLastMessageTimestampMs;
    if (gapMs < kDataChannelInactivityTimeoutMs) {
      return;
    }

    logger.error(
      "[WebRTC] data channel inactivity timeout"
      + " gapMs=" + gapMs
      + " thresholdMs=" + kDataChannelInactivityTimeoutMs
      + " readyState=" + webrtc.remoteDataChannel.readyState
      + " iceState=" + state);

    clearDataChannelWatchdog();
    beginReconnect(webrtc.lastTimestampMs, webrtc.activeConfig, "datachannel-inactivity");
  }, kDataChannelWatchdogIntervalMs);
}

function resetIceDiagnostics() {
  clearIceConnectivityTimer();
  clearDataChannelWatchdog();
  webrtc.sentIceCandidatesCount = 0;
  webrtc.receivedIceCandidatesCount = 0;
  webrtc.remoteDescriptionTimestampMs = null;
  webrtc.remoteDataChannelLastMessageTimestampMs = null;
  webrtc.localCandidateTypeCounts = createCandidateTypeCounters();
  webrtc.remoteCandidateTypeCounts = createCandidateTypeCounters();
}

function isAttemptTokenActive(attemptToken) {
  return attemptToken === webrtc.activeAttemptToken;
}

function setAttemptPhase(phase, context) {
  if (webrtc.attemptPhase == phase) {
    return;
  }

  logger.log(
    "[ICE] attempt phase " + webrtc.attemptPhase + " -> " + phase
    + (context ? " (" + context + ")" : ""));
  webrtc.attemptPhase = phase;
}

function scheduleIceConnectivityTimer(peerConnection) {
  clearIceConnectivityTimer();

  webrtc.iceConnectivityTimerId = setTimeout(() => {
    if (peerConnection !== webrtc.peerConnection) {
      return;
    }

    const state = peerConnection.iceConnectionState;
    if (state == 'connected' || state == 'completed') {
      return;
    }

    logger.error(
      '[ICE] connectivity timeout'
      + ' timeoutMs=' + kIceConnectivityTimeoutMs
      + ' iceState=' + state
      + ' sentCandidates=' + webrtc.sentIceCandidatesCount
      + ' receivedCandidates=' + webrtc.receivedIceCandidatesCount
      + ' queuedCandidates=' + webrtc.pendingIceCandidates.length);

    logCandidateTypeSummary("timeout");
    if (webrtc.remoteCandidateTypeCounts.srflx == 0 && webrtc.remoteCandidateTypeCounts.relay == 0) {
      logger.warn(
        "[ICE] timeout diagnostics: remote candidate set has no srflx/relay entries"
        + " (host-only remote candidates).");
    }
    if (webrtc.localCandidateTypeCounts.srflx == 0 && webrtc.localCandidateTypeCounts.relay == 0) {
      logger.warn(
        "[ICE] timeout diagnostics: local candidate set has no srflx/relay entries."
        + " Configure config.json iceServers with reachable STUN/TURN.");
    }

    logIceCandidatePairStats('timeout', peerConnection);
    logAllIceCandidatePairs('timeout', peerConnection);
    if (peerConnection === webrtc.peerConnection) {
      setAttemptPhase("idle", "connectivity-timeout");
    }
  }, kIceConnectivityTimeoutMs);
}

async function logIceCandidatePairStats(reason, peerConnection) {
  if (!peerConnection || peerConnection !== webrtc.peerConnection) {
    return;
  }

  try {
    const stats = await peerConnection.getStats();
    const statsById = {};
    let selectedPair = null;

    stats.forEach(report => {
      statsById[report.id] = report;
    });

    stats.forEach(report => {
      if (report.type == "transport" && report.selectedCandidatePairId) {
        const pair = statsById[report.selectedCandidatePairId];
        if (pair) {
          selectedPair = pair;
        }
      }
    });

    if (!selectedPair) {
      stats.forEach(report => {
        if (report.type == "candidate-pair" && report.selected) {
          selectedPair = report;
        }
      });
    }

    if (!selectedPair) {
      stats.forEach(report => {
        if (report.type == "candidate-pair" && report.nominated) {
          selectedPair = report;
        }
      });
    }

    if (!selectedPair) {
      stats.forEach(report => {
        if (report.type == "candidate-pair" && report.state == "succeeded") {
          selectedPair = report;
        }
      });
    }

    if (!selectedPair) {
      logger.warn('[ICE] ' + reason + ': no selected candidate pair in getStats()');
      return;
    }

    const localCandidate = statsById[selectedPair.localCandidateId];
    const remoteCandidate = statsById[selectedPair.remoteCandidateId];

    logger.log(
      '[ICE] ' + reason
      + ' selectedPair'
      + ' state=' + selectedPair.state
      + ' nominated=' + selectedPair.nominated
      + ' bytesSent=' + selectedPair.bytesSent
      + ' bytesReceived=' + selectedPair.bytesReceived
      + ' currentRtt=' + selectedPair.currentRoundTripTime);

    if (localCandidate) {
      logger.log(
        '[ICE] localCandidate'
        + ' type=' + localCandidate.candidateType
        + ' protocol=' + localCandidate.protocol
        + ' address=' + localCandidate.address + ':' + localCandidate.port);
    }

    if (remoteCandidate) {
      logger.log(
        '[ICE] remoteCandidate'
        + ' type=' + remoteCandidate.candidateType
        + ' protocol=' + remoteCandidate.protocol
        + ' address=' + remoteCandidate.address + ':' + remoteCandidate.port);
    }
  } catch (error) {
    logger.error('[ICE] Failed to collect ICE stats:', error);
  }
}

function formatStatsCandidate(candidate) {
  if (!candidate) {
    return "unknown";
  }

  const address = candidate.address || candidate.ip || "unknown";
  const port = candidate.port || "unknown";
  const type = candidate.candidateType || "unknown";
  const protocol = candidate.protocol || "unknown";

  return address + ":" + port + " type=" + type + " protocol=" + protocol;
}

async function logAllIceCandidatePairs(reason, peerConnection) {
  if (!peerConnection || peerConnection !== webrtc.peerConnection) {
    return;
  }

  try {
    const stats = await peerConnection.getStats();
    if (peerConnection !== webrtc.peerConnection) {
      return;
    }

    const statsById = {};
    const pairs = [];

    stats.forEach(report => {
      statsById[report.id] = report;
      if (report.type == "candidate-pair") {
        pairs.push(report);
      }
    });

    if (pairs.length == 0) {
      logger.warn("[ICE] " + reason + " full candidate-pair dump: no candidate-pair entries.");
      return;
    }

    const states = {};
    let succeededPairs = 0;

    logger.log("[ICE] " + reason + " full candidate-pair dump: count=" + pairs.length);

    pairs.forEach(pair => {
      const state = pair.state || "unknown";
      states[state] = (states[state] || 0) + 1;
      if (state == "succeeded") {
        succeededPairs += 1;
      }

      const localCandidate = statsById[pair.localCandidateId];
      const remoteCandidate = statsById[pair.remoteCandidateId];

      logger.log(
        "[ICE] pair"
        + " id=" + pair.id
        + " state=" + state
        + " nominated=" + (pair.nominated || false)
        + " selected=" + (pair.selected || false)
        + " bytesSent=" + (pair.bytesSent || 0)
        + " bytesReceived=" + (pair.bytesReceived || 0)
        + " currentRtt=" + (pair.currentRoundTripTime != null ? pair.currentRoundTripTime : "n/a")
        + " requestsSent=" + (pair.requestsSent || 0)
        + " requestsReceived=" + (pair.requestsReceived || 0)
        + " responsesSent=" + (pair.responsesSent || 0)
        + " responsesReceived=" + (pair.responsesReceived || 0)
        + " local=(" + formatStatsCandidate(localCandidate) + ")"
        + " remote=(" + formatStatsCandidate(remoteCandidate) + ")");
    });

    const stateSummary = Object.entries(states)
      .map(([state, count]) => state + "=" + count)
      .join(" ");
    logger.log("[ICE] " + reason + " candidate-pair states: " + stateSummary);

    if (succeededPairs == 0) {
      logger.warn("[ICE] " + reason + " diagnostics: no succeeded candidate pairs.");
    }
  } catch (error) {
    logger.error("[ICE] Failed to dump all candidate pairs:", error);
  }
}

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
  webrtc.activeConfig = config;
  webrtc.peerConnectionConfig = resolvePeerConnectionConfig(config);
  beginReconnect(urlParams.get('position'), config, "initialize-webrtc");
}

function beginReconnect(position, config, reason) {
  const effectiveConfig = config || webrtc.activeConfig;
  if (!effectiveConfig) {
    logger.error("[ICE] Missing WebRTC config, unable to initialize or reconnect.");
    return;
  }

  const deviceId = effectiveConfig.deviceId || null;
  const isDuplicateInitialize = reason == "initialize-webrtc"
    && deviceId
    && deviceId == webrtc.activeDeviceId
    && (webrtc.attemptPhase == "starting" || webrtc.attemptPhase == "connected");
  const isDuplicateRestart = reason != "initialize-webrtc"
    && webrtc.attemptPhase == "restarting";

  if (isDuplicateInitialize) {
    logger.warn(
      "[ICE] Suppressing duplicate initialize-webrtc for active device."
      + " deviceId=" + deviceId
      + " phase=" + webrtc.attemptPhase);
    return;
  }

  if (isDuplicateRestart) {
    logger.warn(
      "[ICE] Suppressing duplicate reconnect request while restarting."
      + " reason=" + reason
      + " deviceId=" + (deviceId || "unknown"));
    return;
  }

  webrtc.activeConfig = effectiveConfig;
  if (deviceId) {
    webrtc.activeDeviceId = deviceId;
  }
  webrtc.lastInitReason = reason || "unknown";
  webrtc.activeAttemptToken += 1;

  const attemptToken = webrtc.activeAttemptToken;
  const phase = reason == "initialize-webrtc" ? "starting" : "restarting";
  setAttemptPhase(phase, "reason=" + webrtc.lastInitReason + " token=" + attemptToken);

  reconnect(position, effectiveConfig, attemptToken);
}

function reconnect(position, effectiveConfig, attemptToken) {
  if (!isAttemptTokenActive(attemptToken)) {
    logger.warn("[ICE] Ignoring reconnect for stale attempt token=" + attemptToken);
    return;
  }

  elements.pauseButton.textContent = "||";
  webrtc.pauseValue = false;
  resetIceDiagnostics();
  closePeerConnection();
  closeWebSocket();
  webrtc.peerConnection = null;
  webrtc.peerConnectionAttemptToken = 0;
  webrtc.remoteDataChannel = null;
  webrtc.remoteDescriptionSet = false;
  webrtc.pendingIceCandidates = [];
  webrtc.attemptId += 1;
  logger.log(
    "[ICE] Starting connection attempt #"
    + webrtc.attemptId
    + " token=" + attemptToken
    + " deviceId=" + (webrtc.activeDeviceId || "unknown")
    + " reason=" + webrtc.lastInitReason);
  mse.buffersArray = new Array();
  const queryString = window.location.search;
  const urlParams = new URLSearchParams(queryString);
  const speed = urlParams.get('speed');
  webrtc.deliveryMethod = (urlParams.get('deliveryMethod') == 'mse') ? 'mse' : 'srtp';
  if (webrtc.deliveryMethod == 'srtp') {
    elements.pauseButton.disabled = false;
  }

  function post(path, data = {}) {
    return fetch('https://' + effectiveConfig.vmsHost + ':' + effectiveConfig.vmsPort + path,
      {
        method: 'POST',
        headers:
        {
          'Content-Type': 'application/json',
          "Authorization": "Bearer " + effectiveConfig.adminSessionToken
        },
        body: JSON.stringify(data)
      })
      .then(r => r.json())
  }

  function initWebSocket(url) {
    if (!isAttemptTokenActive(attemptToken)) {
      logger.warn("[ICE] stale attempt ignored before websocket init token=" + attemptToken);
      return;
    }

    const ws = new WebSocket(url);
    serverConnection = ws;

    ws.onopen = () => {
      if (!isAttemptTokenActive(attemptToken) || ws !== serverConnection) {
        logger.log("[WebSocket] stale onopen ignored token=" + attemptToken);
        return;
      }
      logger.log('[WebSocket] connection established token=' + attemptToken);
    };

    ws.onerror = (error) => {
      if (!isAttemptTokenActive(attemptToken) || ws !== serverConnection) {
        logger.log("[WebSocket] stale onerror ignored token=" + attemptToken);
        return;
      }
      logger.error("[WebSocket] error=", error);
    };

    ws.onclose = (event) => {
      if (!isAttemptTokenActive(attemptToken) || ws !== serverConnection) {
        logger.log("[WebSocket] stale onclose ignored token=" + attemptToken + " code=" + event.code);
        return;
      }

      logger.log(
        '[WebSocket] connection closed'
        + ' code=' + event.code
        + ' clean=' + event.wasClean
        + (event.reason ? ' reason=' + event.reason : '')
        + ' token=' + attemptToken);
      setAttemptPhase("idle", "websocket-closed");
    };

    ws.onmessage = message => gotMessageFromServer(message, ws, attemptToken);
  }

  function onTicketReceived(token) {
    if (!isAttemptTokenActive(attemptToken)) {
      logger.warn("[ICE] stale attempt ignored after ticket response token=" + attemptToken);
      return;
    }

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

    let url = 'wss://'
      + effectiveConfig.vmsHost
      + ':'
      + effectiveConfig.vmsPort
      + '/rest/v3/devices/'
      + effectiveConfig.deviceId
      + '/webrtc'
      + '?_ticket=' + token
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
      + (panoramic != null ? ('&panoramic=' + panoramic) : '')
      + '&sendTimestampIntervalMs=100';

    logger.log('Url: ' + url);
    initWebSocket(url);
  }

  post('/rest/v4/login/tickets', {})
    .then(r => {
      logger.log('Got token: ' + r.token + " tokenAttempt=" + attemptToken);
      onTicketReceived(r.token);
    })
    .catch(e => {
      if (!isAttemptTokenActive(attemptToken)) {
        logger.warn("[ICE] stale attempt ignored after ticket error token=" + attemptToken);
        return;
      }
      logger.error('Login error', e);
      setAttemptPhase("idle", "ticket-fetch-failed");
    });
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
    logger.error('No Media Source API available');
    return;
  }
  mse.mediaSource = new MediaSource();
  elements.remoteVideo.src = window.URL.createObjectURL(mse.mediaSource);
  mse.mediaSource.onsourceopen = () => {
    logger.log('ms is opened');
    if (mse.mimeType)
      startSourceBuffer();
  };
}

function appendToBuffer()
{
  if (mse.sourceBuffer && mse.buffersArray.length > 0) {
    logger.log('ms add from buffers');
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
    logger.warn('mime is not supported: ' + mse.mimeType);
    return;
  }
  logger.log('start source buffer');
  mse.sourceBuffer = mse.mediaSource.addSourceBuffer(mse.mimeType);
  mse.sourceBuffer.mode = "sequence";

  updateMseAudio();
  appendToBuffer();
  mse.sourceBuffer.onupdateend = () => {
    logger.log('ms update end');
    appendToBuffer();
  };
  mse.sourceBuffer.onerror = event => {
    logger.error('ms update error ' + event);
    reconnectHandler(event);
  };

  // FIXME: is not correct way to start play, see https://goo.gl/LdLk22
  var playPromise = elements.remoteVideo.play();
  if (playPromise != undefined) {
    playPromise.then(_ => {
      logger.log('play started');
    })
    .catch(error => {
      logger.error('error with play started: ' + error);
      reconnectHandler(error);
    });
  }
}

function start(attemptToken) {
  const currentAttemptToken = attemptToken || webrtc.activeAttemptToken;
  if (!isAttemptTokenActive(currentAttemptToken)) {
    logger.warn("[ICE] Ignoring start() for stale attempt token=" + currentAttemptToken);
    return;
  }

  closePeerConnection();

  const effectivePeerConnectionConfig = webrtc.peerConnectionConfig || peerConnectionConfig;
  const peerConnection = new RTCPeerConnection(effectivePeerConnectionConfig);
  webrtc.peerConnection = peerConnection;
  webrtc.peerConnectionAttemptToken = currentAttemptToken;
  resetIceDiagnostics();
  webrtc.remoteDescriptionSet = false;
  webrtc.pendingIceCandidates = [];
  logger.log(
    "[ICE] Created RTCPeerConnection with config="
    + JSON.stringify({
      iceTransportPolicy: effectivePeerConnectionConfig.iceTransportPolicy || "all",
      iceCandidatePoolSize: effectivePeerConnectionConfig.iceCandidatePoolSize || 0,
      iceServers: sanitizeIceServersForLog(effectivePeerConnectionConfig.iceServers || [])
    }));

  peerConnection.onicecandidate = gotIceCandidate;
  peerConnection.ontrack = gotRemoteStream;
  peerConnection.oniceconnectionstatechange = () => {
    const state = peerConnection.iceConnectionState;
    logger.log(
      '[ICE] state=' + state
      + ' sentCandidates=' + webrtc.sentIceCandidatesCount
      + ' receivedCandidates=' + webrtc.receivedIceCandidatesCount
      + ' queuedCandidates=' + webrtc.pendingIceCandidates.length);

    if (state == 'connected' || state == 'completed') {
      if (isAttemptTokenActive(currentAttemptToken)) {
        setAttemptPhase("connected", "ice-state=" + state);
      }
      clearIceConnectivityTimer();
      logCandidateTypeSummary("connected");
      logIceCandidatePairStats('connected', peerConnection);
    } else if (state == 'failed') {
      if (isAttemptTokenActive(currentAttemptToken)) {
        setAttemptPhase("idle", "ice-state=failed");
      }
      clearIceConnectivityTimer();
      logCandidateTypeSummary("failed");
      logIceCandidatePairStats('failed', peerConnection);
    }
  };
  peerConnection.onconnectionstatechange = () => {
    logger.log('[WebRTC] connectionState=' + peerConnection.connectionState);
  };
  peerConnection.onsignalingstatechange = () => {
    logger.log('[WebRTC] signalingState=' + peerConnection.signalingState);
  };
  peerConnection.onicegatheringstatechange = () => {
    logger.log('[ICE] gatheringState=' + peerConnection.iceGatheringState);
  };
  peerConnection.onicecandidateerror = event => {
    logger.error(
      '[ICE] candidate error'
      + ' host=' + event.hostCandidate
      + ' url=' + event.url
      + ' code=' + event.errorCode
      + ' text=' + event.errorText);
  };

  peerConnection.addEventListener('datachannel', event => {
    logger.log('got remote data channel');
    webrtc.remoteDataChannel = event.channel;
    webrtc.remoteDataChannel.binaryType = 'arraybuffer';
    webrtc.remoteDataChannelLastMessageTimestampMs = Date.now();
    startDataChannelWatchdog(peerConnection);

    webrtc.remoteDataChannel.onopen = () => {
      if (peerConnection !== webrtc.peerConnection) {
        return;
      }

      logger.log(
        "[WebRTC] remote data channel open"
        + " label=" + webrtc.remoteDataChannel.label
        + " id=" + webrtc.remoteDataChannel.id);
      webrtc.remoteDataChannelLastMessageTimestampMs = Date.now();
      startDataChannelWatchdog(peerConnection);
    };

    webrtc.remoteDataChannel.onclose = () => {
      if (peerConnection !== webrtc.peerConnection) {
        return;
      }

      logger.error("[WebRTC] remote data channel closed; scheduling reconnect.");
      reconnectHandler(new Error("remote data channel closed"), peerConnection);
    };

    webrtc.remoteDataChannel.onerror = (event) => {
      if (peerConnection !== webrtc.peerConnection) {
        return;
      }

      logger.error("[WebRTC] remote data channel error; scheduling reconnect event=", event);
      reconnectHandler(new Error("remote data channel error"), peerConnection);
    };

    webrtc.remoteDataChannel.addEventListener('message', event => {
      webrtc.remoteDataChannelLastMessageTimestampMs = Date.now();

      if (typeof(event.data) === 'string') {
        var message = JSON.parse(event.data);
        var timestampMs = parseInt(message.timestampMs);
        window.mainProcess.ipc.send("update-timestamp", timestampMs);
        webrtc.lastTimestampMs = timestampMs;
        var currentTimestampMs = Math.floor(Date.now());
        var diff = currentTimestampMs - timestampMs;
        logger.log('dc message: ' + event.data + ' timestampMs diff: ' + diff);
        elements.timestamp.textContent = event.data;
        // Possible status:
        // 100 (before seek),
        // 200 (after successful seek or stream change)
        // 301 (reconnect),
        // 404 (after seek, data for this timestamp is not found)
        if (message.status && message.status == 301) {
          beginReconnect(timestampMs, webrtc.activeConfig, "status-301");
        } else if (message.status && message.status == 100) {
          if (webrtc.deliveryMethod != null && webrtc.deliveryMethod == 'mse') {
            // Note that initial segment can be received before 200, so restarting MSE on 100.
            restartMse();
          }
        }
      } else {
        var buffer = new Uint8Array(event.data);
        logger.log('dc binary: type = ' + typeof(event.data) +  ' len = ' + buffer.length);
        var buffer = new Uint8Array(event.data);
        if (mse.sourceBuffer == null || mse.sourceBuffer.updating) {
          mse.buffersArray.push(buffer);
          logger.log('ms pushed to buffer array, length = ' + mse.buffersArray.length);
        } else if (!document[elements.hiddenName]) {
          try {
            mse.sourceBuffer.appendBuffer(buffer);
          } catch(error) {
            reconnectHandler();
          }
        } else {
            logger.log("skip buffer on hidden page");
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
  logger.log('send stream: ' + webrtc.streamValue);
  webrtc.remoteDataChannel.send(JSON.stringify({'stream': webrtc.streamValue}));
}

function closePeerConnection() {
  clearDataChannelWatchdog();

  if (webrtc.remoteDataChannel) {
    try {
      webrtc.remoteDataChannel.onopen = null;
      webrtc.remoteDataChannel.onclose = null;
      webrtc.remoteDataChannel.onerror = null;
      webrtc.remoteDataChannel.onmessage = null;
      if (webrtc.remoteDataChannel.readyState !== "closed") {
        webrtc.remoteDataChannel.close();
      }
    } catch (error) {
      logger.warn('Error while closing data channel:', error);
    }
    webrtc.remoteDataChannel = null;
  }

  if (!webrtc.peerConnection) {
    webrtc.peerConnectionAttemptToken = 0;
    return;
  }

  try {
    clearIceConnectivityTimer();
    webrtc.peerConnection.onicecandidate = null;
    webrtc.peerConnection.ontrack = null;
    webrtc.peerConnection.oniceconnectionstatechange = null;
    webrtc.peerConnection.onconnectionstatechange = null;
    webrtc.peerConnection.onsignalingstatechange = null;
    webrtc.peerConnection.onicegatheringstatechange = null;
    webrtc.peerConnection.onicecandidateerror = null;
    webrtc.peerConnection.close();
  } catch (error) {
    logger.warn('Error while closing peer connection:', error);
  }

  webrtc.peerConnectionAttemptToken = 0;
}

function closeWebSocket() {
  if (!serverConnection) {
    return;
  }

  serverConnection.onopen = null;
  serverConnection.onerror = null;
  serverConnection.onclose = null;
  serverConnection.onmessage = null;

  try {
    serverConnection.close();
  } catch (error) {
    logger.warn('Error while closing websocket:', error);
  }

  serverConnection = null;
}

function drainPendingIceCandidates(peerConnection) {
  if (!webrtc.remoteDescriptionSet || webrtc.pendingIceCandidates.length === 0) {
    return;
  }

  const pendingIceCandidates = webrtc.pendingIceCandidates.slice();
  webrtc.pendingIceCandidates = [];
  logger.log('[ICE] Draining queued candidates count=' + pendingIceCandidates.length);

  pendingIceCandidates.forEach(candidate => {
    peerConnection.addIceCandidate(candidate)
      .then(() => {
        logCandidate('applied queued remote', candidate);
      })
      .catch(error => {
        logger.error('[ICE] Failed to apply queued candidate:', error);
        reconnectHandler(error, peerConnection);
      });
  });
}

function gotMessageFromServer(message, sourceWebSocket, attemptToken) {
  if (!isAttemptTokenActive(attemptToken)) {
    logger.warn("[ICE] stale websocket message ignored token=" + attemptToken);
    return;
  }

  if (sourceWebSocket !== serverConnection) {
    return;
  }
  if(!webrtc.peerConnection) start(attemptToken);

  const peerConnection = webrtc.peerConnection;

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
    logger.log('[WebRTC] received SDP type=' + signal.sdp.type);
    peerConnection.setRemoteDescription(new RTCSessionDescription(signal.sdp)).then(function() {
      if (peerConnection !== webrtc.peerConnection) {
        return;
      }

      webrtc.remoteDescriptionSet = true;
      if (!webrtc.remoteDescriptionTimestampMs) {
        webrtc.remoteDescriptionTimestampMs = Date.now();
      }
      logger.log(
        '[ICE] Remote description set'
        + ' signalingState=' + peerConnection.signalingState
        + ' queuedCandidates=' + webrtc.pendingIceCandidates.length);
      scheduleIceConnectivityTimer(peerConnection);
      drainPendingIceCandidates(peerConnection);

      // Only create answers in response to offers
      if(signal.sdp.type == 'offer') {
        peerConnection.createAnswer().then(description => createdDescription(description, peerConnection))
          .catch(error => errorHandler(error, peerConnection));
      }
    }).catch(error => errorHandler(error, peerConnection));
  } else if(signal.ice) {
    const candidate = new RTCIceCandidate(signal.ice);
    const candidateInfo = parseCandidateInfo(candidate);
    webrtc.receivedIceCandidatesCount += 1;
    incrementCandidateTypeCounter("remote", candidateInfo.type);
    logCandidate('received remote', candidate);
    if (candidateInfo.type == "host" && !isLikelyIpAddress(candidateInfo.address)) {
      logger.warn(
        "[ICE] Received remote host candidate with non-IP address (likely mDNS)."
        + " address=" + candidateInfo.address + " port=" + candidateInfo.port);
    }

    if (!webrtc.remoteDescriptionSet || !peerConnection.remoteDescription) {
      webrtc.pendingIceCandidates.push(candidate);
      logger.log('[ICE] Queued remote candidate queueSize=' + webrtc.pendingIceCandidates.length);
      return;
    }

    peerConnection.addIceCandidate(candidate)
      .then(() => {
        logCandidate('applied remote', candidate);
      })
      .catch(error => {
        logger.error('[ICE] Failed to apply remote candidate:', error);
        reconnectHandler(error, peerConnection);
      });
  }
}

function gotIceCandidate(event) {
  if (webrtc.peerConnectionAttemptToken !== webrtc.activeAttemptToken) {
    logger.log(
      "[ICE] Ignoring local candidate from stale peer connection."
      + " peerConnectionToken=" + webrtc.peerConnectionAttemptToken
      + " activeToken=" + webrtc.activeAttemptToken);
    return;
  }

  if(event.candidate == null) {
    logger.log('[ICE] Local candidate gathering finished');
    logCandidateTypeSummary("local-gathering-complete");
    return;
  }

  const candidateInfo = parseCandidateInfo(event.candidate);
  webrtc.sentIceCandidatesCount += 1;
  incrementCandidateTypeCounter("local", candidateInfo.type);
  logCandidate('generated local', event.candidate);

  if (serverConnection && serverConnection.readyState === WebSocket.OPEN) {
    serverConnection.send(JSON.stringify({'ice': event.candidate}));
    logger.log('[ICE] Sent local candidate over signaling channel');
  } else {
    logger.warn('[ICE] Local candidate generated but signaling channel is not open');
  }
}

function createdDescription(description, peerConnection) {
  if (peerConnection !== webrtc.peerConnection) {
    return;
  }

  logger.log('got description');

  peerConnection.setLocalDescription(description).then(function() {
    if (peerConnection !== webrtc.peerConnection) {
      return;
    }

    if (serverConnection && serverConnection.readyState === WebSocket.OPEN) {
      serverConnection.send(JSON.stringify({'sdp': peerConnection.localDescription}));
    }
  }).catch(error => reconnectHandler(error, peerConnection));
}

function gotRemoteStream(event) {
  logger.log('got remote stream');
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

function errorHandler(error, peerConnection) {
  if (peerConnection && peerConnection !== webrtc.peerConnection) {
    return;
  }

  logger.error("[WebRTC] error=", error);
  reconnectHandler(error, peerConnection);
}

function reconnectHandler(error, peerConnection) {
  if (peerConnection && peerConnection !== webrtc.peerConnection) {
    return;
  }

  logger.error('[WebRTC] reconnect requested due to error:', error);
  if (peerConnection) {
    logIceCandidatePairStats('before-reconnect', peerConnection);
  }
  beginReconnect(webrtc.lastTimestampMs, webrtc.activeConfig, "reconnect-error");
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

    logger.log(`Scaled= ${JSON.stringify(scaled)}`);

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
        logger.log("State changed, re-rendering state");
        renderState(data);
    });

    window.mainProcess.ipc.on('initialize-webrtc', (event, config) => {
        logger.log("Frontend: Initializing WebRTC config=", config);
        initWebRtc(config);
    });
}

const onReady = () => {
    initNotifications();
    initCanvas();
    initControls();
};

document.addEventListener("DOMContentLoaded", onReady);
