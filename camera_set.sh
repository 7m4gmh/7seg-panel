#!/usr/bin/bash

# フォーカスをオフにして固定
v4l2-ctl -d /dev/video1 -c focus_automatic_continuous=0
v4l2-ctl -d /dev/video1 -c focus_absolute=460

# 露出をオフにして固定
v4l2-ctl -d /dev/video1 -c auto_exposure=1
v4l2-ctl -d /dev/video1 -c exposure_time_absolute=157

v4l2-ctl -d /dev/video1 -c white_balance_automatic=0
v4l2-ctl -d /dev/video1 -c contrast=40 


