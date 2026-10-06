import io

from conftest import TINY_JPEG

# ---- status / pokemon -------------------------------------------------------


def test_status(client):
    r = client.get("/api/v1/status")
    assert r.status_code == 200
    b = r.json()
    assert b["status"] == "online"
    assert b["service"] == "pokedex-backend"
    assert b["version"] == "0.2.0"


def test_pokemon_list(client):
    r = client.get("/api/v1/pokemon")
    assert r.status_code == 200
    b = r.json()
    assert b["total"] >= 151  # At least Kanto
    assert len(b["items"]) == 20  # Default limit
    assert b["items"][0]["id"] == 1
    assert b["items"][0]["name"] == "Bulbasaur"
    assert b["page"] == 1 and b["limit"] == 20


def test_pokemon_list_pagination(client):
    r = client.get("/api/v1/pokemon", params={"page": 2, "limit": 10})
    b = r.json()
    assert b["page"] == 2
    assert b["limit"] == 10
    assert len(b["items"]) == 10
    assert b["items"][0]["id"] == 11  # 10 per page, page 2 starts at 11


def test_pokemon_detail_4(client):
    r = client.get("/api/v1/pokemon/4")
    assert r.status_code == 200
    b = r.json()
    assert b["id"] == 4
    assert b["name"] == "Charmander"
    assert b["types"] == ["fire"]
    assert b["height"] == 0.6 and b["weight"] == 8.5
    # 6 stats
    s = b["stats"]
    assert s["hp"] == 39
    assert s["attack"] == 52
    assert s["defense"] == 43
    assert s["special_attack"] > 0
    assert s["special_defense"] > 0
    assert s["speed"] == 65
    assert b["sprite_small"] == "/api/v1/assets/pokemon/0004/small"
    assert b["sprite_large"] == "/api/v1/assets/pokemon/0004/large"


def test_pokemon_25_pikachu(client):
    r = client.get("/api/v1/pokemon/25")
    assert r.status_code == 200
    b = r.json()
    assert b["name"] == "Pikachu"
    assert b["types"] == ["electric"]
    assert b["stats"]["hp"] == 35
    assert b["stats"]["speed"] == 90


def test_pokemon_404(client):
    assert client.get("/api/v1/pokemon/99999").status_code == 404


def test_search_by_name(client):
    r = client.get("/api/v1/pokemon", params={"search": "char"})
    names = [i["name"] for i in r.json()["items"]]
    assert "Charmander" in names
    assert "Charmeleon" in names
    assert "Charizard" in names
    assert "Bulbasaur" not in names


def test_search_by_id(client):
    r = client.get("/api/v1/pokemon", params={"search": "25"})
    ids = [i["id"] for i in r.json()["items"]]
    assert 25 in ids


def test_type_filter(client):
    r = client.get("/api/v1/pokemon", params={"type": "water"})
    b = r.json()
    assert b["total"] > 0
    for item in b["items"]:
        assert "water" in item["types"]


def test_kanto_complete(client):
    """All 151 Kanto Pokémon must be accessible."""
    for i in range(1, 152):
        r = client.get(f"/api/v1/pokemon/{i}")
        assert r.status_code == 200, f"Pokemon {i} not found"


def test_kanto_6stats(client):
    """All Kanto Pokémon must have 6 non-zero stats."""
    for i in range(1, 152):
        b = client.get(f"/api/v1/pokemon/{i}").json()
        s = b["stats"]
        assert s["hp"] > 0, f"#{i} HP=0"
        assert s["attack"] > 0, f"#{i} ATK=0"
        assert s["defense"] > 0, f"#{i} DEF=0"
        assert s["special_attack"] > 0, f"#{i} SP_ATK=0"
        assert s["special_defense"] > 0, f"#{i} SP_DEF=0"
        assert s["speed"] > 0, f"#{i} SPD=0"


# ---- assets -----------------------------------------------------------------


def test_asset_small(client):
    r = client.get("/api/v1/assets/pokemon/1/small")
    assert r.status_code == 200
    assert r.headers["content-type"] in ("image/png", "application/octet-stream")
    assert len(r.content) > 100


def test_asset_large(client):
    r = client.get("/api/v1/assets/pokemon/25/large")
    assert r.status_code == 200
    assert r.headers["content-type"] in ("image/png", "application/octet-stream")
    assert len(r.content) > 100


def test_asset_404(client):
    assert client.get("/api/v1/assets/pokemon/99999/small").status_code == 404


def test_manifest(client):
    r = client.get("/api/v1/assets/manifest")
    assert r.status_code == 200
    b = r.json()
    assert b["pokemon_count"] >= 151
    assert b["asset_format"] == "png"
    assert len(b["pokemon"]) >= 151


def test_data_version(client):
    r = client.get("/api/v1/data/version")
    assert r.status_code == 200
    b = r.json()
    assert b["pokemon_count"] >= 151


# ---- devices ----------------------------------------------------------------


def test_device_report(client):
    r = client.post("/api/v1/devices/status", json={
        "device_id": "pokedex-camera-01",
        "device_type": "camera",
        "status": "online",
        "ip": "192.168.1.50",
    })
    assert r.status_code == 200
    b = r.json()
    assert b["ok"] is True
    assert b["device"]["device_id"] == "pokedex-camera-01"
    assert b["device"]["last_seen"] is not None


# ---- full scan pipeline -----------------------------------------------------


def test_full_scan_pipeline(client):
    # 1) display requests a scan
    r = client.post("/api/v1/scan/request",
                    json={"device_id": "pokedex-display-01"})
    assert r.status_code == 200
    scan_id = r.json()["scan_id"]
    assert r.json()["status"] == "waiting_camera"

    # 2) camera polls → pending
    r = client.get("/api/v1/camera/pending",
                   params={"device_id": "pokedex-camera-01"})
    assert r.json() == {"pending": True, "scan_id": scan_id}

    # 3) no second pending scan
    r = client.get("/api/v1/camera/pending",
                   params={"device_id": "pokedex-camera-01"})
    assert r.json()["pending"] is False

    # 4) camera uploads JPEG (multipart)
    r = client.post(f"/api/v1/scan/{scan_id}/image",
                    data={"device_id": "pokedex-camera-01"},
                    files={"image": ("test.jpg", io.BytesIO(TINY_JPEG), "image/jpeg")})
    assert r.status_code == 200

    # 5) display polls result → MOCK VISION → Pikachu
    r = client.get(f"/api/v1/scan/{scan_id}")
    b = r.json()
    assert b["status"] == "complete"
    assert b["pokemon"]["id"] == 25
    assert b["pokemon"]["name"] == "Pikachu"
    assert b["confidence"] == 0.97


def test_unknown_scan_404(client):
    assert client.get("/api/v1/scan/doesnotexist").status_code == 404
