# Источник географических данных

Границы: geoBoundaries, gbOpen, UKR ADM1, boundaryID `UKR-ADM1-14850775`.
27 административных единиц: 24 области, АР Крым, Киев и Севастополь.
Исходный набор описывает границы за 2017 год и не является картой текущего контроля территорий.

- Метаданные: https://www.geoboundaries.org/api/current/gbOpen/UKR/ADM1/
- Исходный файл: https://github.com/wmgeolab/geoBoundaries/raw/9469f09/releaseData/gbOpen/UKR/ADM1/geoBoundaries-UKR-ADM1_simplified.geojson
- Источники набора: OpenStreetMap, Wambacher.
- © OpenStreetMap contributors: https://www.openstreetmap.org/copyright
- Лицензия базы данных: Open Data Commons Open Database License 1.0 (ODbL).
- Полный текст лицензии: https://opendatacommons.org/licenses/odbl/1-0/

В `metadata.json` сохранены исходные метаданные, в `ukraine-oblasts.geojson` — исходная геометрия.
`src/MapData.cpp` — производная геометрическая база на условиях ODbL, полученная скриптом `tools/generate-map.js`.
Изменения: проекция координат с коэффициентом cos(48°) по долготе, украинские подписи, ручные позиции подписей.
Исходные кольца, включая внутренние границы, сохранены; дополнительных упрощений геометрии нет.
Коды ISO в исходном наборе исторические. Для подключения API нельзя использовать их без сверки с поставщиком.
