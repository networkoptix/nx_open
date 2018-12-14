# P2P Socket
These classes represent a transport layer for Nx peer-to-peer communication. It uses a websocket
connection (a preferred way of doing things) or two separate http connections (in case if the
websocket connection fails for some reasons) internally.