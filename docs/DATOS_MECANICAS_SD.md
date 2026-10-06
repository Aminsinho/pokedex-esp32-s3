# Datos SD para generaciones, evoluciones y movimientos

## Fuente y alcance

El generador obtendrá datos de PokéAPI v2 durante la preparación en el PC:

- `/pokemon/{id}`: movimientos y `version_group_details`.
- `/pokemon-species/{id}`: generación de introducción, preevolución y URL de
  cadena evolutiva.
- `/evolution-chain/{id}`: árbol y condiciones de evolución.
- `/version-group/{name}`: relación entre grupo y generación.
- `/move/{id}`: nombre español y tipo.

PokéAPI nunca será consultada desde el ESP32. La descarga debe usar caché local,
reintentos acotados y un manifiesto reproducible.

## Separación del dataset existente

No se cambia `pokemon_index.bin` ni `data/NNNN.bin` versión 1. Los datos nuevos
se instalan en paralelo:

```text
/pokedex/pokemon/
  mechanics/
    manifest.json
    data/0001.bin
    data/0002.bin
    ...
```

Esta separación permite restaurar la ficha actual y provisionar por intervalos
sin reescribir descripciones, índice ni sprites.

## Formato `PKME` versión 1

Cada `data/NNNN.bin` contiene exclusivamente una especie y puede leerse bajo
demanda.

### Cabecera fija

| Campo | Tipo | Descripción |
|---|---:|---|
| magic | `char[4]` | `PKME` |
| version | `uint16` | `1` |
| pokemon_id | `uint16` | ID nacional |
| payload_size | `uint32` | bytes después de cabecera |
| crc32 | `uint32` | CRC del payload |
| generation_mask | `uint16` | bits 0..8 = generaciones I..IX |
| generation_count | `uint8` | secciones incluidas |
| reserved | `uint8` | cero |

### Cadena evolutiva común

Después de la cabecera:

- `node_count: uint8`
- Por nodo: `species_id: uint16`, `introduced_generation: uint8`,
  `parent_index: int8`, `condition_count: uint8`.
- Por condición: `kind: uint8`, `introduced_generation: uint8`,
  `value_u16: uint16`, `text_len: uint8`, `text_utf8[text_len]`.

`parent_index = -1` identifica la raíz. El árbol permite preevoluciones,
evoluciones posteriores y bifurcaciones sin duplicar sprites.

### Sección por generación

Por cada bit activo, en orden ascendente:

- `generation: uint8`
- `version_group_code: uint8`
- `move_count: uint16`
- Por movimiento: `level: uint8`, `order: uint16`, `move_id: uint16`,
  `type_code: uint8`, `flags: uint8`, `name_len: uint8`,
  `name_utf8[name_len]`.

Todos los enteros son little-endian. Las cadenas son UTF-8 sin terminador. Se
rechazan longitudes que excedan el archivo, IDs fuera de rango, más de 255
nodos, más de 512 movimientos por generación o una máscara incoherente.

## Manifiesto

`mechanics/manifest.json` debe incluir:

- `format: "PKME"`, `version: 1`, fecha y versión del generador.
- SHA-256 del JSON fuente consolidado y número de especies.
- Tabla exacta generación → version group → código binario.
- Conteos de archivos, generaciones, nodos y movimientos.
- Lista de IDs con error o sin learnset; una generación incompleta hace fallar
  la generación del dataset, no se oculta silenciosamente.

## Pipeline de generación

1. Descargar y cachear recursos JSON sin modificar `pokemon.json` vigente.
2. Resolver IDs desde URLs y validar 1..1025.
3. Construir cada árbol, detectar ciclos y filtrar por introducción.
4. Extraer únicamente movimientos `level-up` del grupo canónico.
5. Resolver nombre español y tipo de cada movimiento una sola vez.
6. Escribir primero en staging local, releer, validar CRC y comparar muestras
   contra JSON fuente.
7. Provisionar a SD por rangos (`--first`/`--last`) usando ACK y checksum, como
   las herramientas actuales; escritura atómica `.tmp` → nombre final.
8. Releer desde SD antes de contabilizar un archivo como instalado.

## API de firmware prevista

```cpp
class PokemonMechanicsDatabase {
public:
    bool begin(const char* basePath);
    bool availableGenerations(uint16_t id, uint16_t& mask);
    bool loadEvolution(uint16_t id, uint8_t generation,
                       EvolutionTree& out);
    bool loadLevelMoves(uint16_t id, uint8_t generation,
                        MovePage& out, uint16_t offset, uint8_t limit);
};
```

- Solo la máscara y metadatos pequeños permanecen en RAM durante la ficha.
- Los movimientos se leen por páginas desde SD.
- Los árboles se limitan y se liberan al abandonar la ficha.
- Los buffers grandes, si fueran necesarios, se asignan en PSRAM, nunca en la
  pila de la tarea de UI o red.

## Pruebas necesarias

- Generador determinista con caché y fixture sin red.
- CRC, truncamiento, versión incorrecta y límites maliciosos.
- Mapeo de los nueve version groups canónicos.
- Dedupe y orden de movimientos; tratamiento de nivel cero.
- Evolución lineal, preevolución, bifurcación y especie sin evolución.
- Filtrado de evoluciones introducidas después de la generación elegida.
- UTF-8 español en nombres y condiciones.
- Provisión por rango, reanudación y checksum de relectura.
- Prueba de memoria: cambiar repetidamente especie/generación/pestaña.
- Compilación temprana antes de completar el importador de 1025 especies.

## Migración y rollback

- La primera implementación debe funcionar aunque `mechanics/` no exista.
- No formatear la SD ni reemplazar índice, detalles, audio o sprites.
- Antes de provisionar, respaldar únicamente el directorio `mechanics/` si ya
  existe.
- El rollback consiste en volver al firmware anterior o retirar `mechanics/`;
  la ficha actual permanece legible porque su formato no cambia.

