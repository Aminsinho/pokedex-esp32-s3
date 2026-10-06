#!/usr/bin/env python3
"""QA: verificar que la lista ATAQUES muestra TODOS los movimientos desplazandose.

Provisiona @MECH del id, abre @DETAIL, va a la pestana ATAQUES (tab 3),
captura el inicio (@SNAP), desplaza la lista (@SCROLL +dy) y captura de nuevo.

Uso:
    python tools/qa_scroll.py --id 416 --gen 0 --dy 160
"""
from __future__ import annotations
import argparse
import time
from pathlib import Path
import serial

from qa_tabs import ROOT, send_file, snap, to_png, expect, W, H


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--id", type=int, default=416)
    ap.add_argument("--gen", type=int, default=0, help="indice de seccion (0=primera gen disponible)")
    ap.add_argument("--dy", type=int, default=160, help="desplazamiento vertical en px")
    ap.add_argument("--port", default="COM4")
    args = ap.parse_args()

    mech = ROOT / "sd_dataset" / "pokemon" / "mechanics" / "data" / f"{args.id:04d}.bin"
    if not mech.exists():
        raise SystemExit(f"no existe {mech}")

    outdir = ROOT / "build-sinnoh"
    outdir.mkdir(parents=True, exist_ok=True)

    port = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=5)
    port.dtr = False
    port.rts = False
    port.port = args.port
    with port:
        time.sleep(1.5)
        port.reset_input_buffer()

        print(f"provision @MECH {args.id} ({mech.stat().st_size} b) ...")
        send_file(port, "MECH", args.id, mech)
        print("provision OK")

        port.write(f"@DETAIL {args.id}\n".encode()); port.flush(); time.sleep(0.8)
        port.write(b"@TAB 3\n"); port.flush(); time.sleep(0.5)
        if args.gen != 0:
            port.write(f"@GEN {args.gen}\n".encode()); port.flush(); time.sleep(0.5)

        # snap() escribe @SNAP por sí solo; NO enviar @SNAP antes de llamarlo.
        port.reset_input_buffer()
        data, w, h = snap(port)
        top = outdir / f"qa_scroll_{args.id}_top.png"
        to_png(data, top)
        print(f"TOP -> {top} ({len(data)} raw)")

        # Desplazar la lista y capturar de nuevo (snap() envía su propio @SNAP).
        port.write(f"@SCROLL {args.dy}\n".encode()); port.flush(); time.sleep(0.6)
        data, w, h = snap(port)
        bot = outdir / f"qa_scroll_{args.id}_bottom.png"
        to_png(data, bot)
        print(f"BOTTOM -> {bot} ({len(data)} raw)")

    print("OK")
    return 0


if __name__ == "__main__":
    main()
