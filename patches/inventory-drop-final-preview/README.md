# Podgląd nie pokazuje docelowego miejsca automatycznego upuszczenia

Dotychczasowy podgląd drag&drop pokazywał footprint wskazany kursorem. Gdy był on
zajęty, pozostawał poprawnie czerwony, ale rzeczywiste `SetItem(itm)` wybierało inne
miejsce przez `FindFreeCell()` i gracz nie widział, gdzie przedmiot ostatecznie trafi.

Po poprawce blocked attempted footprint nadal jest czerwony, a równocześnie dokładny
final footprint wybrany przez automatic placement jest zielony. Dla zwykłego
`dpPlace` istniejący pojedynczy zielony footprint pozostaje bez zmian, podobnie jak
jednoznaczny target `dpMerge` i semantyka reference-list/quick-slotów.

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
rysowane dwukrotnie.

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
`RemoveItem`, `SetItem`, `PlaceItemAtPos`, `DrawDropPreview` oraz odpowiednią ścieżkę
reference-list. Sprawdza między innymi wolne i zajęte miejsce, czerwony attempted i
zielony final, pełne footprinty wielokomórkowe, same-list remove/drop, vertical
placement, auto-grow, merge, quick-slot replacement oraz clipping po przewinięciu.

Pakiet zweryfikowano na czystym upstreamie
`6c793faee008d83f86cec2429d39d7aa39b5bc66`: bez zależności `apply.py` odmawia, a po
kolejnym nałożeniu trzech wcześniejszych pakietów przechodzą check, apply, ponowne
rozpoznanie poprawki i komplet testów. Wynikowe `src/` i `tests/inventory-drop/` są
równoważne gałęzi źródłowej.

CI po wypchnięciu 15 września 2026: **zielone**. Dla commita źródłowego
`44cd856ff914d736dfdb8a4a0812d3b1c1bb0a83` przeszły
[Build engine](https://github.com/styczu/ixray-1.6-stcop/actions/runs/35026875498) i
[Non-Unity build](https://github.com/styczu/ixray-1.6-stcop/actions/runs/35026875437).
Dla integracyjnego stanu `build/tmz` `e955faff29a6439ba211c5a34b157b3efea4bc23`
przeszły [Build engine](https://github.com/styczu/ixray-1.6-stcop/actions/runs/35026875243)
i [Non-Unity build](https://github.com/styczu/ixray-1.6-stcop/actions/runs/35026875178).

Test w grze: **nie wykonano**.

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
