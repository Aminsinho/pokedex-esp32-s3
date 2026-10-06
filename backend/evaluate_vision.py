"""Small real-model regression set; synthetic scenes are not webcam accuracy."""
import io
import json
import time
import argparse
import httpx
from pathlib import Path
from PIL import Image
from app.services.vision_service import OllamaVisionService

def run():
    parser = argparse.ArgumentParser()
    parser.add_argument("--ids", nargs="+", type=int)
    parser.add_argument("--raw", action="store_true")
    args = parser.parse_args()
    if args.raw:
        original_post = httpx.Client.post
        def logged_post(self, *a, **kw):
            response = original_post(self, *a, **kw)
            print(json.dumps({"model_output": response.json().get("message", {}).get("content")}), flush=True)
            return response
        httpx.Client.post = logged_post
    service = OllamaVisionService()
    cases = args.ids or [1, 4, 7, 26, 39, 52, 94, 133, 143, 392, 393, 448, 658, 778]
    for id in cases + [0]:
        frame = Image.new("RGB", (1280, 720), (205, 210, 215))
        if id:
            with Image.open(Path("storage/assets/large") / f"{id:04}.png") as original:
                sprite = original.convert("RGBA").resize((320, 320))
                frame.paste(sprite, (480, 200), sprite)
        encoded = io.BytesIO()
        frame.save(encoded, "JPEG", quality=95)
        started = time.monotonic()
        try:
            result = service.recognize(encoded.getvalue())
            actual, error = result.pokemon_id, None
        except Exception as exc:
            actual, error = None, str(exc)
        print(json.dumps({"expected": id, "actual": actual, "error": error,
                          "seconds": round(time.monotonic()-started, 2)}), flush=True)

if __name__ == "__main__": run()
