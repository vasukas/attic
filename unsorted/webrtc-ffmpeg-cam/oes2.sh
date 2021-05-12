#!/bin/bash

# Source parameters are specified in 'stream' invocation

# Build server if needed
if [ main.go -nt ffrtc ] && [ ! -e ffrtc.exe ]; then  # -nt: newer than
    go build . || exit 1
    strip ffrtc
fi

Pids=()
Port=10004  # first RTP port, ffmpeg -> browser

function stream {
    WebPort=$1
    shift

    ffmpeg -hide_banner \
        $@ \
        -f rtp -sdp_file /tmp/ffrtc.$Port.sdp \
        rtp://127.0.0.1:$Port \
        >/tmp/ffrtc.ffmpeg.$Port.log 2>&1 &
    Pids+=( $! )

    ./ffrtc -codec video/H264 -webso 127.0.0.1:$WebPort -port $Port &
    Pids+=( $! )
    
    (( Port += 2 ))  # skip RTCP port
}

# First parameter is websocket port, others are passed to ffmpeg
#stream 8081 -f mpegts -i udp://@225.0.1.5:20001
#stream 8082 -f mpegts -i udp://@225.0.1.5:20002

stream 8081 -stream_loop -1 -i low.mp4
stream 8082 -stream_loop -1 -i low.mp4

echo "Press Ctrl+C to kill everything"
( trap exit SIGINT ; read -r -d '' _ </dev/tty )

for pid in ${Pids[@]}; do
    kill $pid
done

echo "DEAD"
