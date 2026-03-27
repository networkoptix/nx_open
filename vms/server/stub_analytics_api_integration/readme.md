# Stub Analytics API Integration

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Purpose

It is an Electron application used to test Analytics API workflow for working with Integrations.

# Install Electron, nodejs and other dependencies

Run the following commands from the application's root directory(the example is for Ubuntu Linux):
```
# Install nodejs
curl -fsSL https://deb.nodesource.com/setup_lts.x | sudo bash -
sudo apt update
sudo apt --assume-yes install nodejs -y

# Install electron
sudo npm install -g --save electron

# Install dependecies
npm install

# Install the 'ws' package
npm install ws
```

# Enable Integration Request creation

To create Integration Requests, the system setting `allowRegisteringIntegrations` must be set
to `true`.

Advanced settings can be set in the Server Admin panel at `/settings/advanced`.

# How to run the application

- Launch VMS Server and the Client.

- Edit the Stub Analytics API Integration config file `config.json`. It's located in the
    application root directory. Fill in the Server address, port, admin username and admin
    password.

- WebRTC ICE configuration.

    The stub now reads optional ICE settings from `config.json` and forwards them to
    `RTCPeerConnection`.

    - `iceServers`: array of STUN/TURN servers (`urls`, optional `username`, optional
      `credential`).
    - `iceTransportPolicy`: optional, `all` (default) or `relay`.
    - `iceCandidatePoolSize`: optional integer.

    Example TURN config:
```
{
    "iceServers": [
        {
            "urls": ["turn:turn.example.com:3478"],
            "username": "turn-user",
            "credential": "turn-password"
        }
    ],
    "iceTransportPolicy": "all"
}
```

    If `iceServers` are not provided, built-in public STUN defaults are used.

- ICE diagnostics.

    Detailed ICE logs are emitted from the renderer and mirrored into main-process logs with
    `[MAIN] Renderer console message ...`.
    On connection timeout, the logs now print candidate-type summaries and explicit warnings
    when both sides are host-only (no `srflx`/`relay`), which is the common cause of
    ICE staying in `checking`.
```
electron .
```
The Stub Analytics API Integration application window will open.

- Create and approve the Integration Request.

    In the Stub Analytics API Integration application window, click "Create Integration request"
    button. This action announces the Integration to the Server. Then click "Approve Integration
    Request" button. This simulates the Server admin's Integration approval. After the approval,
    the file `integration_credentials.json` will be created in the application's root directory.
    This will contains the username and password for logging into the VMS system, and the
    Integration Request id, which coincides with the id of the Integration User - a User created
    specifically for the Integration.

- Connect to the Server via WebSocket.

    In the Stub Analytics API Integration application window click "Connect RPC client" button. Now
    the application is connected to the Server via WebSocket and you can enable/disable its Engine
    in the Client.

- Enable the Engine on a camera.

    After the Integration Request approval, a new Engine, which can be enabled on a camera will be
    created. To verify this, you can open VMS Client, click "Camera Settings" on a camera and
    observe this Integration in the Integration list. You can enable or disable this Engine by
    clicking a slider button in the Client. After enabling the Engine, you can move the rectangle
    in the Stub Analytics API Integration application window and observe the corresponding
    rectangle moving in the Client window.

- To remove the Integration from the Server, click "Remove Integration" button in the Stub
    Analytics API Integration application window.
