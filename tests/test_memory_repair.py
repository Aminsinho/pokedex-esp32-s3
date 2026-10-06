"""Static guardrails for the exact build/stack regressions (hardware QA separate)."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_photo_download_does_not_allocate_frame_on_task_stack():
    source = (ROOT / "src/pokedex/NetworkTask.cpp").read_text(encoding="utf-8")
    method = source.split("static void runGetPhoto(")[1].split("void NetworkTask::runCommand")[0]
    assert "uint8_t buf[PHOTO_SIZE]" not in method
    assert "heap_caps_malloc(PHOTO_SIZE, MALLOC_CAP_SPIRAM" in method
    assert "heap_caps_free(buf)" in method
    assert "result.scanId = scanId" in method


def test_authoritative_build_uses_opi_and_custom_partition():
    rules = (ROOT / "AGENTS.md").read_text(encoding="utf-8")
    fqbn = next(line for line in rules.splitlines() if line.startswith("esp32:esp32:esp32s3:"))
    assert "PSRAM=opi" in fqbn
    assert "PartitionScheme=custom" in fqbn
    assert "0x640000" in (ROOT / "src/pokedex/partitions.csv").read_text()


def test_photo_dimensions_and_stale_scan_guarded():
    source = (ROOT / "src/pokedex/ScanScreen.cpp").read_text(encoding="utf-8")
    assert "w != 96 || h != 96" in source
    assert "lv_img_cache_invalidate_src(&photoDesc)" in source
    bridge = (ROOT / "src/pokedex/pokedex.ino").read_text(encoding="utf-8")
    photo = bridge.split("case NetEvt::SCAN_PHOTO:")[1].split("case NetEvt::SCAN_STATUS:")[0]
    assert "r.scanId == ScanService::scanId()" in photo
