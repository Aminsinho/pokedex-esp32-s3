"""Regressions for name-first recognition and visual reference verification."""
import io
import json
import httpx
import pytest
from PIL import Image
from app.services.vision_service import OllamaVisionService


@pytest.fixture
def frame():
    output = io.BytesIO()
    Image.new("RGB", (1280, 720), "white").save(output, "JPEG")
    return output.getvalue()


def answers(monkeypatch, items):
    calls = []
    def post(self, url, **kwargs):
        payload = kwargs["json"]
        assert "pokemon_id" not in payload["format"].get("properties", {})
        calls.append(payload)
        return httpx.Response(200, request=httpx.Request("POST", url),
                              json={"message": {"content": json.dumps(items.pop(0))}})
    monkeypatch.setattr(httpx.Client, "post", post)
    return calls


@pytest.mark.parametrize("name,id", [("Raichu", 26), ("Infernape", 392), ("Squirtle", 7),
                                     ("Mimikyu", 778), ("Mr. Mime", 122)])
def test_species_id_comes_from_catalogue(monkeypatch, frame, name, id):
    calls = answers(monkeypatch, [{"found": True, "name": name, "confidence": .95}, {"same_species": True}])
    assert OllamaVisionService().recognize(frame).pokemon_id == id
    assert len(calls[1]["messages"][0]["images"]) == 2


@pytest.mark.parametrize("found,name,confidence", [(False, "", 0), (True, "Raichu", .4),
                                                    (True, "Inventedmon", .99)])
def test_reject_without_fake_match(monkeypatch, frame, found, name, confidence):
    calls = answers(monkeypatch, [{"found": found, "name": name, "confidence": confidence}])
    with pytest.raises(ValueError): OllamaVisionService().recognize(frame)
    assert len(calls) == 1


def test_visual_disagreement_is_bounded(monkeypatch, frame):
    reply = {"found": True, "name": "Gyarados", "confidence": 1.0}
    calls = answers(monkeypatch, [reply, {"same_species": False}, reply, {"same_species": False}])
    with pytest.raises(ValueError, match="reference_mismatch"):
        OllamaVisionService().recognize(frame)
    assert len(calls) == 4


def test_second_look_must_be_verified(monkeypatch, frame):
    calls = answers(monkeypatch, [{"found": True, "name": "Gyarados", "confidence": 1.0},
        {"same_species": False}, {"found": True, "name": "Greninja", "confidence": .95},
        {"same_species": True}])
    assert OllamaVisionService().recognize(frame).pokemon_id == 658
    assert len(calls) == 4
