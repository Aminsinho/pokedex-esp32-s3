import json
from pathlib import Path

from app.config import settings
from app.schemas.pokemon import Pokemon


class PokemonService:
    """In-memory Pokémon database backed by backend/app/data/pokemon.json.

    Architecture is ready to be swapped for SQLite/PostgreSQL later:
    only this class needs to change, not the API or the firmware.
    """

    def __init__(self, data_file: Path = settings.data_file):
        self._all: list[Pokemon] = []
        self._by_id: dict[int, Pokemon] = {}
        self._load(data_file)

    def _load(self, path: Path) -> None:
        if not path.exists():
            return
        with open(path, encoding="utf-8") as f:
            raw = json.load(f)
        for item in raw:
            p = Pokemon(**item)
            self._all.append(p)
            self._by_id[p.id] = p
        self._all.sort(key=lambda p: p.id)

    def list_pokemon(self, search: str | None = None, type: str | None = None,
                     page: int = 1, limit: int = 20) -> dict:
        items = self._all
        if search:
            s = search.lower()
            items = [p for p in items if s in p.name.lower() or str(p.id) in search]
        if type:
            t = type.lower()
            items = [p for p in items if t in [x.lower() for x in p.types]]
        total = len(items)
        start = (page - 1) * limit
        return {
            "items": [p.summary() for p in items[start:start + limit]],
            "page": page,
            "limit": limit,
            "total": total,
        }

    def get_pokemon(self, pokemon_id: int) -> dict | None:
        p = self._by_id.get(pokemon_id)
        return p.detail() if p else None

    def search(self, query: str) -> list[dict]:
        q = query.lower()
        return [p.detail() for p in self._all if q in p.name.lower() or str(p.id) in query]

    def get_count(self) -> int:
        return len(self._all)


pokemon_service = PokemonService()
