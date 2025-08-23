#!/bin/bash

# 引数チェック
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <video_file_path> <youtube_stream_key>"
    exit 1
fi

# 引数を変数に格納
VIDEO_FILE="$1"
STREAM_KEY="$2"
RTMP_URL="rtmp://a.rtmp.youtube.com/live2/"

# C++から送られてくるシグナル(SIGINT, SIGTERM)を捕捉するための関数
# この関数が、子プロセスであるgst-launch-1.0を正しく終了させる
cleanup() {
    echo "シグナルを捕捉しました。GStreamerプロセス ($GST_PID) を停止します..."
    # GST_PIDが存在すれば、そのプロセスにSIGINTを送る
    if [ -n "$GST_PID" ]; then
        kill -SIGINT "$GST_PID"
    fi
    exit 0
}

# cleanup関数をシグナルに紐付ける
trap cleanup SIGINT SIGTERM

echo "無限ループ・ストリーミングを開始します (trap & wait版): ${VIDEO_FILE}"

# GStreamerをバックグラウンド(&)で実行し、そのPIDをGST_PID変数に保存する
gst-launch-1.0 -v flvmux name=mux ! rtmpsink location="${RTMP_URL}${STREAM_KEY}" \
  multifilesrc location="${VIDEO_FILE}" loop=true ! qtdemux name=demux \
  demux.video_0 ! queue ! mux.video \
  demux.audio_0 ! queue ! mux.audio &
GST_PID=$!

# "wait" コマンドで、バックグラウンドのGStreamerプロセスが終了するのを待つ
# これにより、このシェルスクリプトはGStreamerが動いている間は終了しない
# 結果として、親のC++プログラムは「Now Playing」を正しく表示し続けることができる
wait "$GST_PID"

echo "ストリームが正常に終了しました。"

