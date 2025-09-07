gst-launch-1.0 -v \
  filesrc location=../led/badapple.mp4 ! decodebin name=d \
  d. ! queue ! videoconvert ! videoscale \
     ! video/x-raw,width=320,height=240,framerate=15/1 \
     ! x264enc tune=zerolatency speed-preset=veryfast bitrate=300 key-int-max=30 bframes=0 \
     ! h264parse config-interval=1 \
     ! rtph264pay pt=96 config-interval=1 \
     ! udpsink host=192.168.10.107 port=9999 \
  d. ! queue ! audioconvert ! audioresample \
     ! audio/x-raw,rate=48000,channels=2 \
     ! opusenc inband-fec=true bitrate=96000 frame-size=20 \
     ! rtpopuspay pt=97 \
     ! udpsink host=192.168.10.107 port=10000

