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
warunki slotów, kosz. Na pasie i szybkich slotach podświetlenie trafia tam, gdzie
przedmiot faktycznie wyląduje, co przy niezerowym `cell_sp_x` nie musi być komórką
pod kursorem: to znany błąd skoku w `PickCell`, opisany w README pakietu
`inventory-drop-cell` i celowo nietknięty.

## Sprawdzone

Patch nakłada się na czysty upstream
`6c793faee008d83f86cec2429d39d7aa39b5bc66` z nałożonym `inventory-drop-cell`,
a po nałożeniu drzewo jest identyczne z gałęzią źródłową. Sprawdzono też, że bez
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

**Nie wykonano buildu Windows Release ani próby w grze, więc wygląd podświetlenia
nie był oglądany na ekranie.** Po kompilacji sprawdź, czy podświetlane komórki
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

`patch.json` zawiera wersje, zależności i sumę SHA-256 eksportu. Przy
aktualizacji IX-Ray najpierw sprawdź patch na nowym checkoucie; zgodność
z nowszym kodem nie jest zagwarantowana.
