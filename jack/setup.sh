#!/bin/bash

set -e

echo "Killing old instances..."
killall jackd

echo "Starting JACK server..."

# Start JACK in background
jackd -d alsa -P hw:1,0 -C hw:2,0 -r 48000 -p 64 -n 2 &
JACK_PID=$!

# Give JACK time to initialize
sleep 2

echo "Starting harmonizer..."

# Start harmonizer in background
./harmonizer 1.5 &
HARMONIZER_PID=$!

# Give harmonizer time to register JACK ports
sleep 2

echo "Waiting for JACK ports..."

# Optional: wait until harmonizer appears in JACK graph
until jack_lsp | grep -q "harmonizer"; do
    sleep 1
done

echo "Connecting JACK ports..."

# Audio routing
jack_connect system:capture_1 harmonizer:in
jack_connect harmonizer:out system:playback_1
jack_connect harmonizer:out system:playback_2

echo "Setup complete."
echo "JACK PID: $JACK_PID"
echo "Harmonizer PID: $HARMONIZER_PID"

echo "Press Ctrl+C to stop..."

# Keep script alive
wait $HARMONIZER_PID
