# Детальные границы веб-карты

Источник: **State Scientific Production Enterprise Kartographia / OCHA Ukraine / OCHA Field Information Services Section**, Ukraine COD-AB v05. Дата границ: 2025-09-01, проверка OCHA: 2025-12-18. Лицензия: **CC BY 3.0 IGO**.

- Набор: https://data.humdata.org/dataset/cod-ab-ukr
- Метаданные: https://data.humdata.org/api/3/action/package_show?id=cod-ab-ukr
- Лицензия: https://creativecommons.org/licenses/by/3.0/igo/legalcode
- Исходный архив: https://data.humdata.org/dataset/d23f529f-31e4-4021-a65b-13987e5cfb42/resource/681beb86-391b-4a08-8140-ca52e80fcdce/download/ukr_admin_boundaries.geojson.zip
- SHA-256 архива: `B8604BC983E1E54A6F7C10186A5CF8474CB8ED49EB038B4BEF8091D92C286790`.

`web/territories.js` — производная геометрия: 27 ADM1, 139 ADM2 и 1769 ADM3. Изменения: совместное упрощение слоёв mapshaper 0.7.101 до 3% с `keep-shapes`, округление координат до 0.00001°, проекция с cos(48°), сокращение атрибутов, присоединение проверенного справочника API. Генерация: `tools/prepare-geodata.ps1`, `tools/generate-details.js`. Это административные границы, а не линии текущего контроля; названия и границы не означают одобрение или признание со стороны ООН.

Сопоставление с UkraineAlarm хранится в `territory-mapping.json`: точные ID, названия и родители, явные переименования и ссылки на проверку. Всего 1579 соответствий: 122 района, 1455 громад и 2 городские записи State (Харьков и Запорожье). Нечёткое сопоставление при запуске не применяется. При изменении ID, названия или родителя детальный статус не назначается чужой геометрии.

Ограничения исходного набора: административная реформа Крыма и Севастополя реализована не полностью; границы ADM3 Севастополя расходятся с ADM1/ADM2. Отсутствие района или громады в справочнике API не означает наличие детальных данных: интерфейс указывает, когда использован только статус всей области. При частичной тревоге в области непроверенная территория остаётся неизвестной.

API ID `1180` сопоставлен сельской Миколаївській громаде `UA5908015` по общедоступной геометрии официальной карты UkraineAlarm: https://map.ukrainealarm.com/data/real-map/regions-standard-v1.geojson?v=c0047abc6afdfb39 . Полигон 1180 содержит центр сельской громады [34.665980, 51.055934] и исключает центр одноимённой поселковой [34.378638, 50.94338]. Использовано только фактическое соответствие идентификаторов; отображаемая геометрия остаётся OCHA. Чернобыльская зона и Севастополь не имеют отдельного regionId в проверенном справочнике UkraineAlarm.

## Исходные границы нативного модуля

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
