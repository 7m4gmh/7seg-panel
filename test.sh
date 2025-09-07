gst-launch-1.0 -v \
udpsrc port=9999 caps="application/x-rtp,media=video,encoding-name=H264,clock-rate=90000,payload=96" \
! rtpjitterbuffer latency=100 \
! rtph264depay ! h264parse \
! avdec_h264 ! videoconvert ! video/x-raw,format=I420 \
! jpegenc quality=45 \
! fakesink -e