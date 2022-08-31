# leap2json
Turn Leap tracking data into json format.
It polls the leapmotion controller for data and then sends it in json format to the target IP and port using UDP.

The project is still a mess as I try to figure out how to transform the rotation matrix to my liking. \
Right now you will get the data as is but maybe it is still useful.

Uses the LeapSDK for Linux version 2.3.1.
https://developer.leapmotion.com/tracking-software-download

## Settings
You can change settings in leap2json.cpp on line 11, 12 and 19.

## How to build
Download SDK, add the include directory of the SDK with -I and then add the lib file.
`g++ -o leap2json -I LeapSDK/include leap2json.cpp LeapSDK/lib/x64/libLeap.so`

## How to run
You need to install leapd. I got if from AUR.
https://aur.archlinux.org/packages/leap-motion-driver

Run leapd `leapd --run` and then start leap2json `./leap2json`.
