#!/bin/bash

# source -> ffmpeg (источник видео)
# Источник должен передавать видео в x264 с форматом yuv420p,
#   остальные форматы не поддерживаются в Chromium-браузерах.
Source=( -f mpegts -i udp://@225.0.1.5:20001 )  # ОЭС

# ffmpeg -> server (адрес сервера принимающего RTP-поток)
RtpHost="127.0.0.1"
RtpPort=10004

# server -> browser
Websocket="0.0.0.0:8081"

# сборка сервера (если требуется)
if [ main.go -nt ffrtc ] && [ ! -e ffrtc.exe ]; then  # -nt: newer than
    go build . || exit 1
    strip ffrtc
fi

# неблокирующий запуск ffmpeg (source -> H.264 -> RTP)
ffmpeg \
    "${Source[@]}" \
    -flags low_delay \
    -c copy \
    -f rtp \
    -sdp_file tmp.sdp `# обязательный параметр, но нам не нужен` \
    rtp://"$RtpHost":"$RtpPort" \
    >tmp-ffmpeg.log 2>&1 &
ffmpeg_pid=$!

# неблокирующий запуск сервера
./ffrtc -codec=video/H264 -webso="$Websocket" -ip="$RtpHost" -port="$RtpPort" &
ffrtc_pid=$!

# ожидаем сигнал завершения
echo "Press Ctrl+C to kill everything"
( trap exit SIGINT ; read -r -d '' _ </dev/tty )  # блокирует поток выполнения до нажатия enter или получения SIGINT

# убиваем фоновые процессы
kill $ffmpeg_pid
kill $ffrtc_pid

echo "DEAD"
