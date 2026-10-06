# Ficha por generaciones, evoluciones y ataques

## Objetivo

Ampliar la ficha local de cada Pokémon con cuatro pestañas táctiles:

1. **FICHA**: sprite, número, nombre, tipos, altura, peso y descripción.
2. **EVOLUCIONES**: cadena completa disponible en la generación seleccionada,
   incluyendo preevoluciones y evoluciones posteriores.
3. **ESTADÍSTICAS**: los seis valores base y sus barras.
4. **ATAQUES**: todos los movimientos aprendidos al subir de nivel en la
   generación seleccionada.

La funcionalidad seguirá siendo SD-first. Una vez provisionados los datos, la
ficha no dependerá del backend, Wi-Fi, Ollama ni PokéAPI.

## Selector de generación

- Aparecerá en las pestañas EVOLUCIONES y ATAQUES, encima del contenido.
- Solo mostrará generaciones con un learnset válido para esa especie.
- Al abrir una ficha se seleccionará la generación disponible más reciente.
- La generación elegida se conserva al cambiar entre EVOLUCIONES y ATAQUES.
- Al abrir otra especie se conserva solo si también está disponible; en caso
  contrario se selecciona su generación más reciente.
- Las generaciones se muestran como `I` a `IX`, con botones anterior/siguiente
  y texto central. No se crearán nueve botones simultáneos en 320×240.

“Disponible” significa que la especie tiene datos de movimientos por nivel en
el grupo de versiones representativo de esa generación. Esto evita enseñar una
generación vacía o inferir disponibilidad únicamente por la fecha de debut.

## Juego representativo por generación

| Generación | Grupo de versiones canónico |
|---|---|
| I | `red-blue` |
| II | `crystal` |
| III | `emerald` |
| IV | `platinum` |
| V | `black-2-white-2` |
| VI | `omega-ruby-alpha-sapphire` |
| VII | `ultra-sun-ultra-moon` |
| VIII | `sword-shield` |
| IX | `scarlet-violet` |

No se unirán learnsets de juegos distintos: una unión puede asignar al mismo
movimiento niveles contradictorios. La tabla será versionada en el generador y
podrá ampliarse más adelante con selector de juego sin cambiar el formato base.

## Pestaña EVOLUCIONES

- Muestra la cadena desde la forma más temprana hasta la última etapa.
- La especie actual queda resaltada.
- Siempre incluye sus ancestros: por ejemplo, la ficha de Raichu muestra
  Pichu → Pikachu → Raichu cuando esas especies existen en la generación.
- También muestra bifurcaciones, como Eevee, mediante una lista vertical.
- Cada fila contiene sprite pequeño SD, nombre y condición resumida en español.
- Pulsar una especie abre su ficha local.
- Se filtran especies cuya generación de introducción sea posterior a la
  seleccionada. Por tanto, una evolución añadida posteriormente no aparece en
  generaciones antiguas.
- Cuando PokéAPI indique el grupo de versiones de una condición, se respeta.
  Si no lo indica, la condición se considera válida desde la introducción de la
  especie destino.

Condiciones contempladas: nivel, objeto, intercambio, amistad, hora, género,
movimiento conocido, tipo conocido, lugar, clima, especie/tipo en el equipo,
objeto equipado, belleza, afecto y condición especial. No se inventará una
condición si la fuente no proporciona datos; se mostrará `Condición especial`.

## Pestaña ATAQUES

- Solo incluye `move_learn_method = level-up`.
- Se ordena por nivel y después por el orden de aprendizaje de la fuente.
- Nivel cero se presenta como `INICIO`.
- Cada fila muestra `Nv.`, nombre español y tipo del movimiento.
- Si un movimiento aparece repetido en el mismo grupo de versiones, se conserva
  una sola entrada `(movimiento, nivel)`.
- La lista será virtual/paginada: no se crearán todos los widgets LVGL a la vez.
- Los nombres se guardan ya traducidos en SD. Si no existe nombre oficial en
  español, se usa el nombre inglés marcado por el generador, nunca una cadena
  vacía.

Potencia, precisión, PP y categoría quedan fuera de esta primera entrega. El
formato reserva flags para poder añadir una pantalla de detalle posteriormente.

## Layout 320×240

- Cabecera existente: 56 px, con VOLVER y favorito.
- Panel de contenido: `y=60`, `h=132` aproximadamente.
- Barra inferior: cuatro botones de unos 76 px de ancho y 40 px de alto:
  `FICHA`, `EVOL.`, `STATS`, `ATAQUES`.
- Los textos inferiores usarán fuente pequeña y área táctil completa.
- El selector de generación ocupa la primera fila del panel en EVOLUCIONES y
  ATAQUES; el resto es contenido desplazable.
- Cambiar de pestaña no vuelve a leer el detalle principal ni el sprite grande.

## Estados y errores

- Archivo mecánico ausente: `DATOS NO INSTALADOS EN SD` solo en las dos pestañas
  nuevas; FICHA y ESTADÍSTICAS siguen funcionando.
- Generación sin datos: no se ofrece en el selector.
- Sprite de un miembro ausente: placeholder, nunca sprite de otra especie.
- Archivo inválido, versión desconocida o CRC erróneo: rechazo completo del
  archivo, mensaje visible y log serie acotado.
- Una pulsación durante carga no puede conservar punteros a datos ya liberados.

## Criterios de aceptación

- Bulbasaur: selector con sus generaciones válidas y learnset distinto entre I
  y IX cuando la fuente lo indique.
- Pichu/Pikachu/Raichu: preevolución y evolución visibles según generación.
- Eevee: bifurcaciones filtradas correctamente por generación.
- Un Pokémon sin evolución: estado claro, sin lista vacía confusa.
- Ataques: totalidad, orden, niveles y nombres comparados con el grupo canónico.
- Navegación especie de evolución → ficha → VOLVER sin fuga ni bloqueo.
- Uso sin Wi-Fi tras provisionar; ningún GET durante navegación de ficha.
- Scroll y cuatro botones probados físicamente en 320×240.

