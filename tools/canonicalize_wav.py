#!/usr/bin/env python3
"""Rewrite a PCM WAV with the canonical 44-byte header used by firmware."""
import sys
import wave
from pathlib import Path

path = Path(sys.argv[1])
with wave.open(str(path), 'rb') as source:
    params = source.getparams()
    frames = source.readframes(source.getnframes())
if (params.nchannels, params.sampwidth, params.framerate, params.comptype) != (1, 2, 44100, 'NONE'):
    raise SystemExit(f'unsupported WAV parameters: {params}')
with wave.open(str(path), 'wb') as target:
    target.setnchannels(1); target.setsampwidth(2); target.setframerate(44100)
    target.writeframes(frames)
