#!/usr/bin/env bash
# Asks F1's live timing hub (no login) for a snapshot of each topic and prints what came back.
# Run during a live session to see which topics are free right now.
# Usage: tools/probe_f1_live.sh [seconds-to-listen]
set -euo pipefail
B=https://livetiming.formula1.com/signalrcore
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
TOPICS='"SessionInfo","TimingData","TimingAppData","DriverList","LapCount","TrackStatus","RaceControlMessages","WeatherData","Position.z","CarData.z"'

TOK=$(curl -s -c "$TMP/jar" -X POST "$B/negotiate?negotiateVersion=1" |
      python3 -c 'import json,sys; print(json.load(sys.stdin)["connectionToken"])')
curl -s -N -b "$TMP/jar" --max-time "${1:-12}" -H "Accept: text/event-stream" "$B?id=$TOK" > "$TMP/sse" &
sleep 1
printf '{"protocol":"json","version":1}\x1e' | curl -s -b "$TMP/jar" -X POST --data-binary @- "$B?id=$TOK" >/dev/null
printf '{"type":1,"invocationId":"0","target":"Subscribe","arguments":[[%s]]}\x1e' "$TOPICS" |
  curl -s -b "$TMP/jar" -X POST --data-binary @- "$B?id=$TOK" >/dev/null
wait || true

python3 - "$TMP/sse" "$TOPICS" <<'PY'
import json, sys
wanted = json.loads('[' + sys.argv[2] + ']')
got, live = {}, {}
for line in open(sys.argv[1]):
    if not line.startswith('data:'): continue
    for m in line[5:].strip().split('\x1e'):
        if not m: continue
        j = json.loads(m)
        if j.get('type') == 3: got.update(j.get('result') or {})
        if j.get('type') == 1:  # streamed update during a live session
            for a in [j.get('arguments') or []]:
                if a: live[a[0]] = live.get(a[0], 0) + 1
for t in wanted:
    snap = f"{len(json.dumps(got[t])):>7} bytes" if t in got else "   MISSING"
    print(f"{t:22} snapshot {snap}   live updates: {live.get(t, 0)}")
PY
