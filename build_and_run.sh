#!/bin/bash

echo "Building SeeWhatYouHear VST3 plugin..."
cd build
cmake --build . --target SeeWhatYouHear_VST3 --verbose

echo "Starting Ardour with VST3 path..."
export VST3_PATH="/home/amits/vst3:$VST3_PATH" # Local host artifacts location
ardour &

echo "VST3 plugin installed at: ~/vst3/SeeWhatYouHear.vst3/"
echo "Plugin should appear in Ardour's VST3 category"
