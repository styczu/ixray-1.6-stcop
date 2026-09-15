# Podgląd komórek docelowych przy przeciąganiu

Rozszerzenie pakietu `inventory-drop-cell`. Podczas przeciągania podświetla
komórki, które przedmiot zajmie: jedną dla apteczki 1x1, dwie dla broni 2x1, cały
blok dla kombinezonu. Zielono, gdy przedmiot się tam zmieści, czerwono, gdy
miejsce jest zajęte i zadziała automatyczne rozmieszczenie.

**Wymaga wcześniejszego nałożenia `inventory-drop-cell`.** Oba patche ruszają inne
pliki, więc `git am` nałożyłby ten samodzielnie i zostawił błąd komórki docelowej;
`apply.py` sprawdza zależność i odmawia. Bez tamtej poprawki podgląd byłby
zgodny sam ze sobą, ale pokazywałby przesuniętą komórkę.

## Co zmienia patch

Żeby podgląd nie mógł skłamać, decyzja została wyjęta z `SetItem` do
`CUIDragDropListEx::PredictDrop`. `SetItem` woła teraz tę funkcję zamiast własnej
kopii warunków, a podgląd pyta ją o dokładnie tę samą pozycję, którą dostanie
`SetItem`, czyli `CUIDragItem::GetPosition()`. Zachowanie samego upuszczania nie
zmienia się.

Dwa parametry domyślne w kontenerze, oba potrzebne tylko podglądowi:

- `IsRoomFree(..., ignore)` - przeciągany przedmiot wciąż zajmuje swoje komórki,
  bo usuwa go dopiero `RemoveItem` w `OnItemDrop`. Bez tego broń 2x1 przesuwana
  o jedno pole zawsze pokazywałaby "zajęte".
- `FindSimilar(..., skip)` - przy przeciąganiu wewnątrz jednej listy przedmiot
  nadal jest jej dzieckiem, więc `R_ASSERT(i != itm)` wywaliłby grę. Ścieżka
  rozmieszczania nie podaje `skip` i asercję zachowuje.

`CUIDragDropReferenceList` ma własne `SetItem`, które podmienia zawartość komórki
zamiast jej odmówić, więc dostaje własne `PredictDrop` - inaczej zajęty szybki
slot świeciłby na czerwono, choć przedmiot i tak by tam wszedł.

Rysowanie w `CUICellContainer::DrawDropPreview`, po ikonach, żeby podświetlenie
zajętej komórki nie schowało się pod ikoną. Modulacja istniejącej tekstury siatki
kolorem wierzchołka - bez nowych shaderów, tekstur i bez zmian w `gamedata`.
Kolory to dwie stałe na górze `UIDragDropListEx.cpp`. Pomijane są listy
z `virtual_cells`, gdzie przedmiot i tak jest centrowany, oraz listy
jednokomórkowe, czyli kosz o komórce 340x768.

Trzy pary plików źródłowych (`UIDragDropListEx.{h,cpp}`,
`UIDragDropReferenceList.{h,cpp}`) i test
`tests/inventory-drop/test_drop_preview.py`.

## Czego podgląd nie pokazuje

Pozycję w siatce, a nie to, czy zasady gry pozwolą na ruch.
`CUIActorMenu::OnItemDrop` może odrzucić przedmiot niezależnie - `CanPutInRuck`,
warunki slotów, kosz. Podświetlenie zawsze pokazuje to, co zrobi `SetItem`, więc
gdyby `PickCell` kiedykolwiek wskazało nie tę komórkę, podgląd pokaże ten sam
błąd - i to jest właściwe zachowanie, bo mówi prawdę o wyniku. Jedyny znany taki
przypadek to ostatni piksel ostatniej komórki pasa, opisany w README pakietu
`inventory-drop-cell`.

## Sprawdzone

Patch nakłada się na czysty upstream
`6c793faee008d83f86cec2429d39d7aa39b5bc66` z nałożonym `inventory-drop-cell`,
a po nałożeniu drzewo jest identyczne z gałęzią źródłową `feature/inventory-drop-preview`
(commit `15e6b828d`). Ostatnio sprawdzone 15 września 2026 przy odbudowie łańcucha
gałęzi inventory. Sprawdzono też, że bez
zależności `apply.py` odmawia, a na repozytorium z już nałożoną poprawką zgłasza
jej obecność. Dołączony test przechodzi w świeżo zapatchowanym checkoucie:
kompiluje produkcyjne `PredictDrop`, `PickCell`, `ValidCell`, `IsRoomFree`,
`FindSimilar` i `GetItemPos`, i sprawdza wolną komórkę, zajętą komórkę, kursor
poza siatką, nachodzenie na własne komórki przeciąganego przedmiotu,
wielokomórkowy obrys, obrys wychodzący poza siatkę, zamianę osi na liście
pionowej oraz łączenie w stos wraz z pominięciem samego siebie.

Kompiluje też produkcyjne `Draw` i `DrawDropPreview` z podstawionym rendererem.
Podgląd liczy komórki bezwzględnie, a pętla siatki od pierwszej widocznej, więc
test sprawdza, że oba przebiegi rysują ten sam kwadrat na tych samych
współrzędnych - dla listy zwykłej, przewiniętej, z odstępami między komórkami
i dla przedmiotu 2x1 - oraz że podświetlenia nie ma tam, gdzie nie powinno.
To pilnuje samego przeliczania współrzędnych; wyglądu na ekranie nie sprawdza.
Test wymaga Pythona 3 i g++ z ASan/UBSan; LeakSanitizer jest domyślnie wyłączony.

Kompilacja na Windowsie przeszła. Na commicie `eae6a01c3` zielone są oba
workflow: `Build engine` w RelWithDebInfo oraz `Non-Unity build` w Debug,
RelWithDebInfo i Release. Ta druga konfiguracja ma tu znaczenie: `FindSimilar`,
które ten patch zmienia, ma w środku gałąź `#ifdef DEBUG`, a kompiluje ją
wyłącznie `Non-Unity build` w Debug.

**Próba w grze dotyczy buildów integracyjnych, nie samego pakietu.** Ekwipunek z tym
kodem sprawdzono w grze przy 2560x1440. Były to buildy z tym kodem i zmianami panelu:
m.in. `7fcb532af` (dawna gałąź `feature/inventory-cell-grid`), a 15 września 2026
`236edf3a7` z `build/tmz`, na którym ekwipunek wyglądał poprawnie. Nie ma zapisu, które
z punktów poniżej przejrzano. Buildu z samych pakietów nałożonych na czysty upstream
nie uruchamiano w grze. Po kompilacji sprawdź, czy podświetlane komórki
zgadzają się z tym, gdzie przedmiot ląduje, i czy kolory są czytelne na twoim
`ui_grid` - jeśli nie, zmień `kDropPreviewFree` i `kDropPreviewBlocked` na górze
`src/xrGame/ui/UIDragDropListEx.cpp`. Przejdź plecak, pas, szybkie sloty, handel,
sloty wyposażenia i kosz.

## Użycie

Ustaw `PAKIET` na ścieżkę tego katalogu, a `IXRAY` na osobny, czysty checkout
silnika z nałożonym już pakietem `inventory-drop-cell`.

```bash
python3 "$PAKIET/apply.py" --repo "$IXRAY"
python3 "$PAKIET/apply.py" --repo "$IXRAY" --apply
python3 "$IXRAY/tests/inventory-drop/test_drop_preview.py"
```

Pierwsze polecenie tylko sprawdza zgodność, w tym obecność zależności. Drugie
nakłada patch jako commit; rozpoznaje też poprawkę już obecną.

Obecność poprawki nie jest rozpoznawana po nałożeniu `inventory-cell-grid`. Tamten
pakiet przepisuje linie `inventory-drop-cell`, więc sprawdzenie zależności odmówi
(kod 2). O stanie takiego repozytorium mówi `apply.py` pakietu `inventory-cell-grid`.

`patch.json` zawiera wersje, zależności i sumę SHA-256 eksportu. Przy
aktualizacji IX-Ray najpierw sprawdź patch na nowym checkoucie; zgodność
z nowszym kodem nie jest zagwarantowana.
