# UI metálica inspirada en las referencias del usuario

Pantalla real 320x240. Adaptación LVGL, no reducción de una ilustración: los
controles siguen siendo widgets táctiles. Carcasa roja, metal azul/gris, visor
turquesa, botones rojos/azules con gradiente y Poké Ball decorativa.

- HOME: sprite del registro, nombre/número/tipo y REGISTRO/SCAN.
- REGISTRO: 12 entradas por página, filas 52 px, miniaturas SD de 48 px;
  PREV/NEXT siguen en 100x36. Sin buscador. Favoritos se refrescan al volver.
- FICHA: imagen SD 96 px del ID abierto (incluido resultado de cámara), dos
  tipos, altura/peso y descripción desplazable. Pestaña ESTADISTICAS con los
  seis valores y barras limitadas a 255. Los datos largos no tapan controles.
- SCAN: visor simbólico (no vídeo), estados reales y sprite al identificar.
  Conserva el flujo C920/Ollama ya aceptado; no muestra porcentajes inventados.

No se añaden controles de formas, sincronización ni búsqueda que no estén
implementados. VOLVER conserva 56 px de altura; ancho 100 px para dejar título.

## Imágenes y memoria

`CatalogSprite` conserva un buffer por imagen usando el asignador LVGL/PSRAM.
La ficha y HOME usan 96x96; las 12 miniaturas reutilizan sus buffers entre
páginas. No hay HTTP en la carga UI. Los datos R565 existentes tienen cabecera
8 bytes; se validan dimensión y tamaño antes de utilizarlos. Falta/corrupción
oculta el sprite anterior y muestra placeholder, nunca un Pokémon diferente.

El formato antiguo había aplanado el canal alfa a negro. Se adapta únicamente
el negro conectado al borde al color del panel; no recupera el alfa original.
Para corregir los bordes del conversor antiguo y completar la caché, usar
`python tools/provision_sprites.py --first 1 --last 1025`. Convierte los PNG
locales del catálogo a RGB565 componiendo alfa por canal sobre el fondo de UI.
Solo reemplaza la caché generada small/large de SD; conserva PNG, dataset y NVS.
El firmware valida tamaño y confirma checksum tras releer cada archivo escrito.
Transferencia USB en bloques confirmados para respetar el búfer CDC de 256 bytes.
Reiniciar después para refrescar los sprites que ya estaban cargados en widgets.

## Compilar sin interferir con servicios

Usar `build-sinnoh`, no `build` (allí siguen logs abiertos de cámara/backend).
FQBN vigente en CURRENT_TASK. Logs de build en raíz: `sinnoh-build.log`.

## Captura real de píxeles por USB

```powershell
python tools/capture_ui.py ui-captures/home.png --view HOME
python tools/capture_ui.py ui-captures/list.png --view LIST
python tools/capture_ui.py ui-captures/pikachu.png --detail 25
python tools/capture_ui.py ui-captures/scan.png --view SCAN
```

Requiere firmware nuevo y COM4 libre. No lee la webcam. Comandos USB @HOME,
@LIST, @SCAN, @DETAIL n y @SNAP permiten QA de navegación/dibujo; no prueban
el sensor táctil físico. @SCAN solo abre esa pantalla, no toma ninguna foto.
El framebuffer de 150 KiB se asigna solo durante @SNAP y se libera al enviarlo;
esa transferencia diagnóstica pausa brevemente el loop. No se usa en operación
normal. Las capturas corresponden a píxeles de LVGL enviados al LCD, no fotos
del panel ni maquetas.
