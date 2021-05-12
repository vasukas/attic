**original name: ffrtc**

RTP stream to browser demo

oes.sh runs:
* ffmpeg, which receives mpegts/H264 yuv420p stream from OES, wraps it in RTP and sends to server
  - script can be modified to send it to non-localhost server
* Go-server, which receives RTP stream and sends it to any number of clients (browsers) via WebRTC+websocket
  - if executable file doesn't exist, it is built (requires Go); executable requires only standard C runtime
  - clients can be non-localhost
* logs are saved to tmp-*.log files in current directory
* no other files from the repo are used

Based on:
* https://github.com/pion/webrtc/tree/master/examples/rtp-to-webrtc
* https://github.com/gorilla/websocket/tree/master/examples/command

Files:
* ffrtc/ffrtc.exe - pre-built executables
* ffrtc.html - demo page 
* ffrtc.js - client-side (browser) script
* go.mod - server Go module file
* go.sum - server Go module dependencies versions (for reproducible builds)
* main.go - server source
* oes.sh - run OES with single stream
* oes2.sh - run OES with two streams
* runme.sh - run webcam demo

Use `./ffrtc --help` to list server options.

Requires ffmpeg to run - `sudo apt ffmpeg`

Requires Go to build server - `sudo apt install golang-go`
