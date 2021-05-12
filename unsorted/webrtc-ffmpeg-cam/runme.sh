#!/bin/bash

# if set, tries to launch one of the known browsers
RunBrowser=1

go build . || exit 1
#strip ffrtc  # remove debug info

if [[ "`uname -s`" == "Linux" ]]; then
    Webcam=( -f video4linux2 -s 640x480 -r 30 -i /dev/video0 )
else
    Webcam=( -f dshow -i video="Integrated Webcam" )
fi

WebCodec="video/H264"
Codec=(
    -c:v libx264 -pix_fmt yuv420p
    -profile:v baseline
    -preset ultrafast
    -tune zerolatency
)

# resize and set size - since webcam can choose on its own
# height must a multiple of two
#Filters=( -vf fps=60,scale=1920:1080 )

# test
#ffmpeg "${Webcam[@]}" -c:v libx264 -preset ultrafast -tune zerolatency -f mpegts udp://127.0.0.1:6666 >tmp-ffudp 2>&1 &
#Webcam=( -i udp://127.0.0.1:6666 )

ffmpeg -hide_banner \
    "${Webcam[@]}" \
    "${Filters[@]}" \
    "${Codec[@]}" \
    -f rtp -sdp_file tmp.sdp \
    rtp://127.0.0.1:10004 \
    >tmp-ffmpeg.log 2>&1 &
ffmpeg_pid=$!
echo "ffmpeg_pid=$ffmpeg_pid"

./ffrtc -codec="$WebCodec" &
ffrtc_pid=$!
echo "ffrtc_pid=$ffrtc_pid"
sleep 2s

HasBrowser=0
if [ "$RunBrowser" -eq 1 ]; then
    Browsers=( chromium firefox google-chrome )
    for b in "${Browsers[@]}"; do
        if [[ ! -z "`which $b`" ]] || [[ -e "$b" ]]; then
            "$b" ffrtc.html >tmp-browser.log 2>&1 &
            HasBrowser=1
            break
        fi
    done
fi
if [[ "$HasBrowser" -ne 1 ]]; then
    echo "Open 'ffrtc.html' in your browser"
fi

echo "Press Ctrl+C to kill everything"
( trap exit SIGINT ; read -r -d '' _ </dev/tty )
kill $ffmpeg_pid
kill $ffrtc_pid
echo "DEAD"
