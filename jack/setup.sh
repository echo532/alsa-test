#!/bin/bash

set -e

cleanup() {
    echo "Ctrl+C detected — shutting down..."

    kill $HARMONIZER_PID 2>/dev/null
    kill $JACK_PID 2>/dev/null
}

trap cleanup INT TERM EXIT

echo "Killing old instances..."
killall jackd 2>/dev/null || true

echo "Starting JACK server..."

jackd -d alsa -P hw:1,0 -C hw:2,0 -r 48000 -p 64 -n 2 &
JACK_PID=$!

sleep 2

echo "Starting harmonizer..."

./harmonizer 1.5 &
HARMONIZER_PID=$!

sleep 2

echo "Waiting for JACK ports..."

until jack_lsp | grep -q "harmonizer"; do
    sleep 1
done

echo "Connecting JACK ports..."

jack_connect system:capture_1 harmonizer:in
jack_connect harmonizer:out system:playback_1
jack_connect harmonizer:out system:playback_2

echo "Setup complete."
echo "Press Ctrl+C to stop..."

wait $HARMONIZER_PID