# Podgląd nie pokazuje docelowego miejsca automatycznego upuszczenia

Dotychczasowy podgląd drag&drop pokazywał footprint wskazany kursorem. Gdy był on
zajęty, pozostawał poprawnie czerwony, ale rzeczywiste `SetItem(itm)` wybierało inne
miejsce przez `FindFreeCell()` i gracz nie widział, gdzie przedmiot ostatecznie trafi.

Po poprawce blocked attempted footprint nadal jest czerwony, a równocześnie dokładny
final footprint wybrany przez automatic placement jest zielony. Dla zwykłego
`dpPlace` istniejący pojedynczy zielony footprint pozostaje bez zmian, podobnie jak
jednoznaczny target `dpMerge` i semantyka reference-list/quick-slotów.

Dwie wcześniejsze wersje poprawnie wyliczały geometrię, lecz próba w grze wykazała,
że podgląd nadal nie trafiał do widocznej ścieżki renderowania. Bieżąca wersja
przenosi jego wywołanie do aktywnego `CUIDragItem::Draw()`: tego samego callbacku,
który przez cały drag rysuje ikonę trzymanego przedmiotu. Lista zachowuje geometrię
i clipping z własnego przebiegu bieżącej klatki, a callback drag itemu rysuje preview
na końcu, więc nie przykrywa go ani zawartość komórek, ani przeciągana ikona.

Przy `DisableInventoryGrid=true` podgląd używa widocznego neutralnego paska
`ui_grid_alt.dds` i kompensuje jego alfę przed nałożeniem czerwonego lub zielonego
koloru. Zwykła tekstura `ui_grid` zachowuje dotychczasową ścieżkę.

**Wymaga wcześniejszego nałożenia `inventory-cell-grid`**, a przez jego zależności także
`inventory-drop-cell` i `inventory-drop-preview`. `apply.py` sprawdza stabilny marker
`CellOffsetUI` wniesiony przez `inventory-cell-grid` i odmawia na czystym upstreamie.

## Co zmienia patch

`SDropPrediction` przenosi rodzaj operacji oraz dwa jawnie nazwane prostokąty:
`attempted_cells` pod kursorem i `final_cells` miejsca faktycznego. `PredictDrop()`
zachowuje dotychczasowe wyniki `dpPlace`, `dpMerge` i `dpAuto`, ale dla `dpAuto`
dodatkowo rozwiązuje finalny target.

Wyszukiwanie row-major first-fit zostało wydzielone do wspólnego resolvera używanego
zarówno przez prediction, jak i produkcyjny `FindFreeCell()`. Obsługuje pełny rozmiar
przedmiotu, vertical placement, auto-grow oraz parametr `ignore`, dzięki któremu stare
komórki przedmiotu przeciąganego w tej samej liście odpowiadają stanowi po
`RemoveItem()`.

Resolver prediction jest czysty: nie wykonuje `Grow()`, `Compact()`, nie zmienia
pojemności ani zawartości listy. Dla auto-grow oblicza logiczną przyszłą pojemność,
a dopiero rzeczywisty `FindFreeCell()` wykonuje potrzebne `Grow()`.

`DrawDropPreview()` przy `dpAuto` rysuje attempted footprint na czerwono i różny final
footprint na zielono. Każdy prostokąt jest osobno przycinany do widocznej części listy,
więc final po przewinięciu trafia na właściwe komórki. Identyczne prostokąty nie są
rysowane dwukrotnie. Geometria listy ma znacznik numeru klatki, więc późniejszy
callback drag itemu nie może użyć nieaktualnych współrzędnych lub clippingu listy,
której w tej klatce nie narysowano. Przy ukrytej siatce preview korzysta z neutralnego,
widocznego paska atlasu zamiast całkowicie przezroczystego paska normalnej komórki.
Jego alfa `102/255` jest kompensowana alfą wierzchołka `240/255`, co daje zamierzone
efektywne `96/255` bez zmiany czerwonego i zielonego RGB.

## Świadome konsekwencje

Produkcja i prediction współdzielą first-fit oraz wzrost listy. Przy vertical placement
usuwa to wcześniejsze podwójne obrócenie rozmiaru w rekurencyjnej ścieżce auto-grow;
rzeczywiste umieszczenie i podgląd używają teraz tego samego poprawnego footprintu.

## Czego patch nie zmienia

Patch nie przebudowuje geometrii inventory, `screen_cell_size`, pixel snapping,
odstępów ani liczby kolumn. Nie dodaje shaderów, tekstur ani zmian w `gamedata`.

Legacy fallback `Compact()` dla pełnej listy o stałym rozmiarze pozostaje bez zmian i
nie jest wykonywany ani symulowany przez podgląd. Jeśli nie ma miejsca bez repackingu,
`final_cells` pozostaje pusty; dzięki temu renderowanie nie modyfikuje inventory co
klatkę.

## Sprawdzone

Wszystkie testy `tests/inventory-drop/` przechodzą z ASan/UBSan. Rozszerzony harness
kompiluje produkcyjne ciała `PredictDrop`, `ResolveFreeCell`, `FindFreeCell`,
`RemoveItem`, `SetItem`, `PlaceItemAtPos`, listowego `DrawDropPreview`,
`CUIDragItem::Draw` oraz odpowiednią ścieżkę reference-list. Sprawdza między innymi
wolne i zajęte miejsce, czerwony attempted i zielony final, pełne footprinty
wielokomórkowe, same-list remove/drop, vertical placement, auto-grow, merge,
quick-slot replacement, clipping po przewinięciu, aktualność geometrii w obrębie
klatki oraz to, że podgląd jest emitowany po przeciąganej ikonie.
Test czyta również rzeczywisty DXT5 `ui_grid_alt.dds`: przypina zerową alfę paska 0,
alfę 102 paska 1, produkcyjny wybór UV oraz kompensację koloru. Osobny przypadek pilnuje
niezmienionej ścieżki zwykłego `ui_grid`.

Pakiet zweryfikowano na czystym upstreamie
`6c793faee008d83f86cec2429d39d7aa39b5bc66`: bez zależności `apply.py` odmawia, a po
kolejnym nałożeniu trzech wcześniejszych pakietów przechodzą check, apply, ponowne
rozpoznanie poprawki i komplet testów. Wynikowe `src/` i `tests/inventory-drop/` są
równoważne gałęzi źródłowej.

CI pierwszej wersji z 15 września 2026: **zielone**. Dla commita źródłowego
`44cd856ff914d736dfdb8a4a0812d3b1c1bb0a83` przeszły
[Build engine](https://github.com/styczu/ixray-1.6-stcop/actions/runs/35026875498) i
[Non-Unity build](https://github.com/styczu/ixray-1.6-stcop/actions/runs/35026875437).
Dla integracyjnego stanu `build/tmz` `e955faff29a6439ba211c5a34b157b3efea4bc23`
przeszły [Build engine](https://github.com/styczu/ixray-1.6-stcop/actions/runs/35026875243)
i [Non-Unity build](https://github.com/styczu/ixray-1.6-stcop/actions/runs/35026875178).

CI poprawionej wersji z 16 września 2026: **zielone**. Dla commita źródłowego
`783e1439d00782f68359fec0cda90bdd2133574f` przeszły
[Build engine](https://github.com/styczu/ixray-1.6-stcop/actions/runs/35030532778) i
[Non-Unity build](https://github.com/styczu/ixray-1.6-stcop/actions/runs/35030532770).
Dla integracyjnego stanu `build/tmz` `58b8903236b997a5b9193903cfa588a11828ef8a`
przeszły [Build engine](https://github.com/styczu/ixray-1.6-stcop/actions/runs/35030533545)
i [Non-Unity build](https://github.com/styczu/ixray-1.6-stcop/actions/runs/35030533528).

CI bieżącej wersji z callbackiem `CUIDragItem::Draw()`: **nie wykonano jeszcze**.

Test w grze: dwie wcześniejsze wersje zostały sprawdzone i nie pokazywały drag
preview. Dla bieżącej wersji z przeniesionym wywołaniem renderowania: **nie wykonano**.

## Użycie

Katalogi czterech pakietów inventory muszą leżeć obok siebie. Na czystym checkoutcie
nakładaj je kolejno:

```sh
python3 patches/inventory-drop-cell/apply.py --repo <ixray> --apply
python3 patches/inventory-drop-preview/apply.py --repo <ixray> --apply
python3 patches/inventory-cell-grid/apply.py --repo <ixray> --apply
python3 patches/inventory-drop-final-preview/apply.py --repo <ixray> --apply
```

Bez `--apply` ostatnie polecenie tylko sprawdza zgodność patcha.
