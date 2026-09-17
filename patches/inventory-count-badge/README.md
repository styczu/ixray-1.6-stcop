# Licznik ilości w kratce ekwipunku pokazuje "x3" zamiast dyskretnej liczby

Samodzielny patch silnika IX-Ray: licznik sztuk tego samego przedmiotu w jednej
kratce ekwipunku (`CUIInventoryCellItem`) drukował zawsze `"x%d"` na stałym,
pełnowymiarowym tle, niezależnie od liczby cyfr. Patch usuwa prefiks "x" i
pokazuje tło dopiero od dwóch cyfr, dopasowane szerokością do tekstu.

## Co zmienia patch

`CUIInventoryCellItem::UpdateItemText()` w `src/xrGame/ui/UICellCustomItems.cpp`:

- format liczby: `"x%d"` → `"%d"`;
- po ustawieniu tekstu: `AdjustWidthToText()` (istniejący helper `CUIStatic`) +
  stały margines, żeby tło nie oblepiało cyfry ciasno;
- tło (`TextureOn`/`TextureOff`) włączone tylko dla liczby ≥ 10 (2+ cyfry),
  jawnie w obu gałęziach `if`/`else` — kratki są poolowane między odświeżeniami
  ekwipunku, więc bez jawnego wyłączenia raz włączone tło zostałoby przy kratce,
  która później dostanie przedmiot z 1-cyfrową ilością.

## Świadome konsekwencje

- **Ten patch sam w sobie nie zmienia niczego widocznego bez towarzyszącej
  zmiany w XML.** Rozmiar/pozycja/font pola tekstowego (`cell_item_text`) i
  próg, od którego tło faktycznie coś zmienia wizualnie, zależą od
  `actor_menu_item_16.xml` — dodatek `ixray-hd-icons` dowozi to jako
  XMLOverride (`configs/ui/mod_actor_menu_item_16_hdicons.xml`, mechanizm
  `AsureXML.cpp`). Bez tego XML-a i tak widać efekt (brak "x", zniknięcie tła
  dla 1 cyfry), ale w vanillowych wymiarach pola.
- **Ważna pułapka, którą to zadanie wcześniej kosztowało rundę:**
  `CUIXml::correct_file_name()` (`xrUI/xrUIXmlParser.cpp`) przy ładowaniu
  `actor_menu_item.xml` w praktyce **zawsze podmienia nazwę na
  `actor_menu_item_16.xml`** na zwykłym widescreen (`ui_base.cpp`,
  `ui_core::get_xml_name`), niezależnie od tego, że kod źródłowy
  (`UICellItem.cpp`) literalnie odwołuje się do nazwy bez sufiksu. Każdy dodatek
  zmieniający wygląd `cell_item_text`/`cell_item_upgrade` musi celować w
  `actor_menu_item_16.xml` (albo XMLOverride z maską
  `mod_actor_menu_item_16_*.xml`), inaczej zmiana jest martwa.
- `CUIAmmoCellItem::UpdateItemText()` (licznik naboi w skrzynkach amunicji, np.
  "31"/"115") **nie jest ruszany** — już wcześniej drukował samą liczbę bez "x"
  i zawsze pokazuje tło. Dzieli ten sam węzeł XML co licznik "xN", więc zmiany
  rozmiaru/fontu w `cell_item_text` obejmują też amunicję, ale próg
  chowania tła dla 1 cyfry go nie dotyczy.

## Czego patch nie zmienia

- Nie rusza `CUICellItem::UpdateItemText()` (bazowa, niewywoływana w praktyce dla
  tych podklas) ani znaczka ulepszenia broni (`cell_item_upgrade`) — to czysta
  zmiana danych XML po stronie dodatku, bez udziału silnika.
- Nie dotyka `tests/inventory-drop/` (inny temat: geometria drag&drop, nie
  formatowanie tekstu licznika).

## Sprawdzone

- CI na gałęzi `fix/inventory-count-badge`: `Build engine` i `Non-Unity build`
  zielone (run `35152309691` / `35152309648`, commit `eb0f3f70a`).
- Zainstalowane przez `install-build.sh`, sha w `xrEngine.exe` potwierdzone.
- **Sprawdzone w grze przez użytkownika**, kilka iteracji rozmiaru/fontu razem
  z `ixray-hd-icons` (`mod_actor_menu_item_16_hdicons.xml`): brak "x", tło
  chowane dla 1 cyfry, docelowy rozmiar czcionki (`ui_font_hdicons_12`, 12pkt)
  zaakceptowany.
- Scalone do `build/tmz` (`--no-ff`, commit `df4380385b08ba67a49b2a6cad3fb54f0812d6a4`).

## Użycie

```bash
python3 PAKIET/apply.py --repo NOWY_IXRAY
python3 PAKIET/apply.py --repo NOWY_IXRAY --apply
```

Pierwsze polecenie wyłącznie sprawdza. Drugie nakłada patch jako jeden commit.
Skrypt rozpoznaje już nałożoną poprawkę, nie zmienia gałęzi, nie pobiera niczego
z sieci i niczego nie wysyła. Po nałożeniu nadal potrzebna jest kompilacja
silnika, instalacja builda i (jeśli chcesz zobaczyć efekt) skopiowanie
`mod_actor_menu_item_16_hdicons.xml` z `ixray-hd-icons` do zainstalowanego
dodatku.

Bez skryptu:

```bash
git -C NOWY_IXRAY apply --check PAKIET/0001-fix-inventory-count-badge.patch
git -C NOWY_IXRAY am --keep-cr --3way PAKIET/0001-fix-inventory-count-badge.patch
```

`--keep-cr` zachowuje zakończenia linii CRLF. W razie konfliktu przejrzyj
`git status`; wycofanie rozpoczętego nakładania to `git am --abort`.

## Aktualizacja patcha

Gałąź `fix/inventory-count-badge` utrzymuj jako samą poprawkę na bazie wybranej
wersji upstreamu — analogicznie do `fix/equipment-condition-time`. Przenieś jej
pojedynczy commit na nową bazę (rebase/cherry-pick) w osobnym checkoucie,
rozwiąż ewentualne konflikty i dopiero wtedy wyeksportuj commit ponownie:

```bash
git format-patch -1 HEAD --stdout --full-index --no-signature > PAKIET/0001-fix-inventory-count-badge.patch
```

Zaktualizuj `patch.json` (`upstream_base`, `commit`, `sha256`) i sprawdź
nałożenie na czystej bazie.
