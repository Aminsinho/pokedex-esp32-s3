#!/usr/bin/env python3
"""Fill missing Spanish descriptions locally with Ollama; preserve existing text."""
import json
from pathlib import Path
import httpx

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / 'backend/app/data/pokemon.json'
records = json.loads(PATH.read_text(encoding='utf-8'))
missing = [p for p in records if not p.get('description_es')]
client = httpx.Client(base_url='http://127.0.0.1:11434', timeout=180)
for start in range(0, len(missing), 10):
    batch = missing[start:start + 10]
    source = [{'id': p['id'], 'name': p['name'], 'text': p['description_en']} for p in batch]
    prompt = ('Traduce al español natural de España estas descripciones de Pokédex. '
              'Conserva exactamente id y name, no añadas información y devuelve solo JSON con '
              'forma {"items":[{"id":899,"text":"..."}]}. Usa Pokémon con tilde.\n' +
              json.dumps(source, ensure_ascii=False))
    response = client.post('/api/chat', json={'model': 'qwen3-vl:8b-instruct', 'stream': False,
        'format': 'json', 'messages': [{'role': 'user', 'content': prompt}],
        'options': {'temperature': 0}})
    response.raise_for_status()
    result = json.loads(response.json()['message']['content'])['items']
    translated = {int(item['id']): item['text'].strip() for item in result}
    expected = {p['id'] for p in batch}
    if set(translated) != expected or any(not translated[i] for i in expected):
        raise RuntimeError(f'invalid translation batch {start}: {set(translated)} != {expected}')
    for pokemon in batch: pokemon['description_es'] = translated[pokemon['id']]
    print(f'{min(start + 10, len(missing))}/{len(missing)}')
if missing:
    PATH.write_text(json.dumps(records, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'filled {len(missing)} missing descriptions')
