gst-launch-1.0 -v filesrc location=/home/radxa/led/mtknsmb2.mp4 ! qtdemux name=demux \
 demux.video_0 ! queue ! h264parse ! mppvideodec ! videoconvert ! video/x-raw,width=640,height=360 ! x264enc tune=zerolatency ! flvmux name=mux ! rtmpsink location='rtmp://a.rtmp.youtube.com/live2/auk9-d9vb-cp3w-x2rz-by5b' \
 demux.audio_0 ! queue ! aacparse ! avdec_aac ! audioconvert ! voaacenc ! mux.

