#!/bin/bash

# copy this script to SamplerBox pi and run allow development for one session only

# remount disk as read/write
mount -o remount,rw / 

#set the time and date to prevent ssl certification errors
date -s "$(curl -s --head http://google.com | grep ^Date: | sed 's/Date: //g')"
