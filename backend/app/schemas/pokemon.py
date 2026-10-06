from pydantic import BaseModel, Field


class Stats(BaseModel):
    hp: int
    attack: int
    defense: int
    special_attack: int = 0
    special_defense: int = 0
    speed: int


class Ability(BaseModel):
    name: str
    slot: int = 1


class Pokemon(BaseModel):
    id: int
    name: str
    name_es: str | None = None
    types: list[str]
    height: float
    weight: float
    base_experience: int = 0
    generation: str = ""
    abilities: list[Ability] = []
    stats: Stats
    description_en: str = ""
    description_es: str | None = None
    sprite_small: str | None = None
    sprite_large: str | None = None

    @property
    def description(self) -> str:
        return self.description_es or self.description_en

    def summary(self) -> dict:
        return {
            "id": self.id,
            "name": self.name,
            "name_es": self.name_es,
            "types": self.types,
            "height": self.height,
            "weight": self.weight,
            "sprite_small": f"/api/v1/assets/pokemon/{self.id:04d}/small",
            "sprite_large": f"/api/v1/assets/pokemon/{self.id:04d}/large",
        }

    def detail(self) -> dict:
        return {
            "id": self.id,
            "name": self.name,
            "name_es": self.name_es,
            "types": self.types,
            "height": self.height,
            "weight": self.weight,
            "base_experience": self.base_experience,
            "generation": self.generation,
            "abilities": [a.model_dump() for a in self.abilities],
            "stats": self.stats.model_dump(),
            "description": self.description,
            "sprite_small": f"/api/v1/assets/pokemon/{self.id:04d}/small",
            "sprite_large": f"/api/v1/assets/pokemon/{self.id:04d}/large",
        }
