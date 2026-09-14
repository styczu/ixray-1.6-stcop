# Komórka docelowa przy upuszczaniu przedmiotu

Samodzielny patch na to, że przenosząc przedmiot w ekwipunku trzeba było puścić
myszkę w prawej dolnej części komórki. Puszczony gdziekolwiek indziej lądował
w komórce obok: w lewo, w górę albo w lewo-górę. Po poprawce przedmiot trafia do
komórki pod kursorem.

## To nie jest wina `inv_scale`

Skalowanie ikon nie ma z tym nic wspólnego. `ScaleIcon` trafia wyłącznie do
prostokąta tekstury w `CUIInventoryCellItem` i do ikon dodatków broni; rozmiar
logiczny bierze się z `CUICellContainer::PlaceItemAtPos` jako
`m_cellSize * GetGridSize()`, a `UICellItem.cpp` ani `UIDragDropListEx.cpp` nigdy
nie widzą `ScaleIcon`. Objaw jest identyczny z `inv_scale` i bez niego.

Przyczyną jest reguła "lewy górny róg decyduje", odziedziczona po oryginalnym
X-Ray. `CUIDragItem::Init` zapamiętuje `m_pos_offset` jako `ikonaLT - kursor`,
`GetPosition()` zwraca `kursor + m_pos_offset`, czyli lewy górny róg ducha ikony,
a `SetItem` wyznacza z tego komórkę przez `PickCell`, czyli
`floor(pozycja / rozmiar_komórki)`. Przy komórce 41x41 i chwycie `g` wewnątrz
ikony przedmiot trafiał do komórki `cx` tylko wtedy, gdy kursor był w przedziale
`[cx*41 + g, cx*41 + 41)` - czyli w prawym dolnym pasku o szerokości `41 - g`.
Chwyt w środku ikony oznaczał prawą dolną połowę komórki.

## Co zmienia patch

`Init` zaokrągla offset chwytu w dół do pełnych komórek i zapisuje wynik
w nowym polu `m_drop_offset`, którego używa `GetPosition()`. Komórką docelową jest
odtąd komórka pod kursorem pomniejszona o chwyconą podkomórkę: dla przedmiotu 1x1
po prostu komórka pod kursorem, dla broni 2x1 chwycona połowa zostaje pod
kursorem. `Draw()` dalej korzysta z surowego `m_pos_offset`, więc duch ikony
rysuje się dokładnie tak jak dotąd - zmienia się tylko to, gdzie przedmiot ląduje.

Poprawka obsługuje wszystkie ścieżki upuszczania, bo każda przechodzi przez
`GetDragItemPosition`: plecak, handel, przeszukanie zwłok, pas i szybkie sloty.

Dwa pliki źródłowe (`src/xrGame/ui/UICellItem.{h,cpp}`) i test
`tests/inventory-drop/test_drop_cell.py`. Bez zmian w `gamedata` i bez nowych
opcji konfiguracyjnych.

Uwaga na przyszłość dla każdego, kto będzie ten kod ruszał: rozmiar komórki musi
być odczytany w `Init` i zapamiętany. `OnItemDrop` woła `RemoveItem`, które zeruje
listę właściciela, zanim zapyta o pozycję upuszczenia, więc `GetPosition()` nie ma
już do niej dostępu. W kodzie stoi to w komentarzu.

## Sprawa obok, nie objęta patchem

`CUICellContainer::PickCell` dzieli przez `cellSize + spacing*(cap-1)/cap`
(dzielenie całkowite), podczas gdy `PlaceItemAtPos` rozstawia komórki co
`cellSize + spacing`. Dla pasa wychodzi 60 zamiast 65, dla szybkich slotów 71
zamiast 81. Plecak i listy handlu mają `spacing = 0`, więc zgłoszonego błędu to
nie dotyczy. Osobna sprawa, celowo poza tym patchem.

Offset chwytu jest zaokrąglany rozmiarem komórki listy **źródłowej**, a komórka
wybierana na liście docelowej. Dla przedmiotów 1x1 nie ma to znaczenia, bo offset
wychodzi zerowy i decyduje sam kursor. Dla przedmiotów wielokomórkowych
przenoszonych między listami o różnym rozmiarze komórki (plecak ma 41 w
`actor_menu.xml` i 33 w `actor_menu_16.xml`) zostaje różnica
`podkomórka * (komórka_źródłowa - komórka_docelowa)`, czyli poniżej ćwierci
komórki - i tak znacznie mniej niż pełny offset chwytu sprzed poprawki.

## Sprawdzone

Patch nakłada się na czysty upstream
`6c793faee008d83f86cec2429d39d7aa39b5bc66` i po nałożeniu drzewo jest identyczne
z gałęzią źródłową. Dołączony test przechodzi w świeżo zapatchowanym checkoucie:
kompiluje produkcyjne ciała `quantize_grab_offset`, `PickCell` i `ValidCell`
i sprawdza 7938 pozycji kursora dla przedmiotu 1x1 przy trzech różnych chwytach,
osobno chwyty 2x1 i 1x2, odtwarza zachowanie sprzed poprawki, a także przypadki
brzegowe - kursor tuż poza ikoną, offset większy od przedmiotu, zerowy rozmiar
komórki. Test wymaga Pythona 3 i g++
z ASan/UBSan. LeakSanitizer jest domyślnie wyłączony, żeby test działał
w środowisku z ograniczonym ptrace.

**Nie wykonano buildu Windows Release ani próby w grze.** Po kompilacji sprawdź
w ekwipunku: przedmiot 1x1 chwycony za środek ma trafić w komórkę pod kursorem,
a broń 2x1 chwycona za prawą połowę ma trafić tak, żeby chwycona połowa była pod
kursorem. Warto przejść plecak, pas, szybkie sloty i handel.

## Użycie

Ustaw `PAKIET` na ścieżkę tego katalogu, a `IXRAY` na osobny, czysty checkout
silnika.

```bash
python3 "$PAKIET/apply.py" --repo "$IXRAY"
python3 "$PAKIET/apply.py" --repo "$IXRAY" --apply
python3 "$IXRAY/tests/inventory-drop/test_drop_cell.py"
```

Pierwsze polecenie tylko sprawdza zgodność. Drugie nakłada patch jako commit;
rozpoznaje też poprawkę już obecną. Potem skompiluj silnik dla swojej wersji.

`patch.json` zawiera wersje i sumę SHA-256 eksportu. Przy aktualizacji IX-Ray
najpierw sprawdź patch na nowym checkoucie; zgodność z nowszym kodem nie jest
zagwarantowana. Zintegrowanej gałęzi nie należy patchować ponownie.

Podświetlanie komórek docelowych jest osobnym pakietem, `inventory-drop-preview`,
i wymaga tego patcha.
