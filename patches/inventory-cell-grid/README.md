# Stały rozmiar komórki ekwipunku w pełnych pikselach ekranu

Po przeskalowaniu UI komórka wypadała na ekranie ułamkowo, na przykład 81.6 px.
W efekcie w plecaku jedne sąsiednie ikony stykały się poprawnie, inne miały piksel
szczeliny, a jeszcze inne lekko na siebie nachodziły. Po poprawce każda lista ma
jeden, stały, całkowity rozmiar komórki i całą geometrię liczy właśnie z niego.

**Wymaga wcześniejszego nałożenia `inventory-drop-cell` i `inventory-drop-preview`.**
Ten patch zmienia rozmiar komórki, którym tamte dwa się posługują: `quantize_grab_offset`
zaokrągla nim chwyt, a podgląd nim rysuje. Nałożony samodzielnie zostawiłby je czytające
rozmiar, którego nikt inny już nie używa; `apply.py` sprawdza obie zależności i odmawia.

Uwaga dla kolejnych pakietów: zależności są tu wykrywane po znaczniku w źródłach
(`m_drop_offset`, `DrawDropPreview`), a **nie** przez `git apply --reverse --check` na
patchu zależności, jak robi `inventory-drop-preview`. Tamten sposób działa tylko wtedy, gdy
oba patche ruszają różne linie. Ten przepisuje linie wniesione przez `inventory-drop-cell`,
więc po jego nałożeniu tamten patch nie daje się już odwrócić — i sprawdzenie zależności
zaczęłoby odmawiać na własnym, poprawnie zapatchowanym repozytorium. Z tego samego powodu
wykrycie "poprawka już obecna" wykonuje się **przed** sprawdzeniem zależności.

## To nie jest wina samej siatki

Tło siatki rasteryzuje się poprawnie. `CUICellContainer::Draw` zaokrągla w dół oba rogi
kwadratu, więc prawa krawędź komórki `k` i lewa komórki `k+1` liczą się z tego samego
wyrażenia i zawsze wychodzą identycznie.

Rozjeżdżają się **ikony**. Każdy `CUICellItem` to `CUIStatic` ze `SetStretchTexture(true)`,
a `CUIStaticItem::RenderInternal` zaokrągla w dół wyłącznie lewy górny róg:

```cpp
UI().ClientToScreenScaled(pos, in_pos.x, in_pos.y);
UI().AlignPixel(pos.x);              // iFloor
...
UI().ClientToScreenScaled(RBp, vSize.x, vSize.y);
RBp.add(pos);                        // RB = podłoga(LT) + przeskalowany rozmiar, bez podłogi
```

Przy kroku 81.6 prawa krawędź pierwszej ikony wypada na 81.6, a lewa drugiej na
`floor(81.6) = 81` — nachodzenie 0.6 px. Dalej prawa drugiej na 162.6, lewa trzeciej na
`floor(163.2) = 163` — szczelina 0.4 px. I tak w kółko, bo część ułamkowa wędruje.

Jeżeli `skala * rozmiar_komórki` jest liczbą całkowitą, odstęp między kolejnymi lewymi
krawędziami jest stały i równy tej liczbie, a prawa krawędź ikony wypada dokładnie na
lewej krawędzi sąsiada. Nie trzeba ruszać globalnej ścieżki renderowania — wystarczy,
żeby lista dobrała sobie taki rozmiar komórki.

## Co zmienia patch

`CUICellContainer` trzyma teraz sześć wartości zamiast dwóch. Surowe `m_cellSizeRaw`
i `m_cellSpacingRaw` to wartości z XML, tak jak dotąd, w jednostkach bazowych 1024x768.
Z nich i z bieżącej skali urządzenia powstaje `m_cellSizeScreen` i `m_cellSpacingScreen`,
czyli `round(wartość * skala)` w **pełnych pikselach ekranu**, a z tych `m_cellSize`
i `m_cellSpacing` z powrotem w jednostkach bazowych, bo tam `CUIWindow` trzyma swoje
współrzędne. Liczy to `UpdateCellMetrics()` i nic poza nim nie zaokrągla.

Pozycję komórki daje wyłącznie `CellOffsetUI()`. Korzystają z niej `SetItemGeometry`
(pozycja i rozmiar `CUICellItem`, wyjęte z `PlaceItemAtPos`), `Draw` przy liczeniu
`drawLT` i ikony tła szybkich slotów. Dzięki temu tło siatki i ikona zaokrąglają
dokładnie tę samą liczbę i nie mogą się rozjechać. Rozmiar ekranowy trafia prosto do
`f_len` i `sp_len` w `Draw`, więc podgląd upuszczania dostaje go bez zmian w swoim kodzie.

`CellSize()` i `CellsSpacing()` zwracają teraz `Fvector2` zamiast `Ivector2`. To celowo
zmiana typu, a nie nowy akcesor obok starego: gdyby obie wersje istniały naraz, wróciłoby
dokładnie to niezależne zaokrąglanie w kilku miejscach, które ma zniknąć. Kompilator
wylicza wszystkie cztery miejsca poza kontenerem, które trzeba było przejrzeć.

`CellOffsetUI` dokłada `kCellPixelBias`, jedną setną piksela ekranu. `RenderInternal`
zaokrągla lewą krawędź w dół, a błąd `float` na drodze `skala * (pozycja + offset)` sięga
przy rozmiarach ekwipunku około 5e-4 px: bez tego narożnik, który powinien wypaść na
równym pikselu, może wyjść o włos poniżej i zejść o piksel za daleko w lewo. Bias wchodzi
do offsetu raz, więc nie rośnie z numerem komórki, i jest sto razy mniejszy od piksela.

## Świadome konsekwencje

**Jeden piksel na pasie znika.** README pakietu `inventory-drop-cell`, sekcja "Sprawa obok",
opisuje, że uśredniony dzielnik w `PickCell` liczy `spacing*(cap-1)/cap` na `int` i przez
to ostatni piksel ostatniej komórki pasa wskazuje komórkę, której nie ma. Tamten pakiet
celowo tego nie ruszał, żeby nie ryzykować konfliktu przy każdym wydaniu. Tutaj tę linię
i tak przepisujemy na jednostki efektywne, więc obcięcie znika samo z siebie. **Sama
struktura dzielnika zostaje uśredniona**, dokładnie tak jak była — to była świadoma
decyzja tamtego pakietu i nie jest przepisywana.

**Podwójne dodanie odstępu w `drawLT`** (`c + 2s` zamiast `c + s`) znika razem
z przepisaniem tej linii. Dziś jest latentne: `TopVisibleCell` zwraca zawsze `x = 0`,
a jedyne listy z niezerowym `cell_sp_y` to pionowe pasy Clear Sky, które nigdy nie
przewijają. Zero obserwowalnej zmiany, ale odnotowane.

**Listy `vertical_placement` zostają jak były.** `SetItemGeometry` używa tam wysokości
komórki na obu osiach ("solution works only for quads cells"), więc ich szerokość dalej
nie jest przyciągnięta do pełnego piksela. Zmiana tego zmieniłaby wygląd slotów
wyposażenia, a te i tak mają `virtual_cells`: przedmiot jest centrowany i nie ma sąsiada,
z którym miałby się stykać. Test pilnuje, żeby to zachowanie nie zmieniło się przypadkiem.

**Siatka może urosnąć o parę pikseli względem `width=` z XML.** Zaokrąglenie w górę dokłada
do pół piksela na komórkę. W `actor_menu_16.xml` plecak ma 7 komórek po 33 (231 jednostek)
w oknie `width="247"`, więc zapas jest duży; pas ma już dziś 241 przy `width="240"`,
a szybkie sloty 228 przy `227`. `GetClientArea` obcina nożycami, więc w najgorszym razie
ostatnia kolumna traci kilka pikseli po prawej.

**Rozmiar w pikselach zależy od rozdzielczości.** Dotąd wszystko było w jednostkach
bazowych i skalowało się samo. Teraz pozycje przedmiotów niosą w sobie skalę, więc
`CUICellContainer::Update` sprawdza, czy skala się zmieniła, i wtedy przelicza metryki,
rozmiar kontenera (`ReinitSize`) oraz pozycje przedmiotów (`RefreshItemsPos`);
`CUIDragDropReferenceList::Update` robi to samo ze swoimi ikonami tła, bo one nie siedzą
wewnątrz kontenera. Sprawdzenie jest w `Update`, a nie w `Draw`, bo `ReinitSize` zeruje
suwak, czyli rodzeństwo rysowane w tej samej klatce. Skutek uboczny: po zmianie
rozdzielczości otwarta lista traci pozycję przewinięcia.

`CellOffsetUI` dzieli przez skalę, więc `UpdateCellMetrics` odrzuca skalę niedodatnią
i zostawia poprzednie metryki. Bez tego urządzenie, które nie zdążyło jeszcze podać
swojego rozmiaru — `ui_core::OnDeviceReset` liczy skalę z `Device.TargetWidth/Height`,
a serwer dedykowany też tworzy `ui_core` — zamieniłoby każdą pozycję przedmiotu
na nieskończoność. Kontener startuje ze skalą `1.0`, żeby dzielnik był użyteczny
od pierwszej chwili.

**`pttLIT`**: `f_len` bierze się wprost z rozmiaru ekranowego, co zakłada, że lista nigdy
nie rysuje się w tym trybie punktów. Sprawdzone — ustawiają go wyłącznie `EliteDetector`
i `DosimeterUI`, czyli ekraniki urządzeń w świecie.

## Czego patch nie zmienia

Wyglądu i skali ekwipunku poza zaokrągleniem: `cell_width`/`cell_height` z XML znaczą to
samo co dotąd, wejście `SetCellSize` dalej bierze `Ivector2`, nie ma nowych opcji ani zmian
w `gamedata`. Nie zmienia się też rozmiar ikony wynikający z `inv_scale` — `ScaleIcon`
trafia wyłącznie do prostokąta tekstury, co opisuje README pakietu `inventory-drop-cell`.

Trzy pary plików źródłowych (`UIDragDropListEx.{h,cpp}`, `UICellItem.cpp`,
`UIDragDropReferenceList.{h,cpp}`) i test `tests/inventory-drop/test_cell_grid.py`.
Dwa istniejące testy zmieniają się razem z typami, które sprawdzają.

## Sprawdzone

Patch nakłada się na czysty upstream `6c793faee008d83f86cec2429d39d7aa39b5bc66`
z nałożonymi `inventory-drop-cell` i `inventory-drop-preview`, a po nałożeniu wszystkie
dotknięte pliki są identyczne z gałęzią źródłową. Sprawdzono też, że bez pierwszej
zależności `apply.py` odmawia, że bez drugiej odmawia tak samo, i że na repozytorium
z już nałożoną poprawką zgłasza jej obecność zamiast nakładać ją drugi raz.

Dołączony test przechodzi w świeżo zapatchowanym checkoucie. Kompiluje produkcyjne
`screen_cell_len`, `UpdateCellMetrics`, `CellOffsetUI`, `SetItemGeometry`,
`RefreshItemsPos`, `ReinitSize`, `PickCell`, `ValidCell`, `GetCellAt`, `TopVisibleCell`,
`GetTexUVLT`, `GetCellsInRange`, `Draw` i `DrawDropPreview`, i sprawdza:

- rozmiar efektywny jest całkowity i równy `round(wartość * skala)`, osobno na każdej osi,
  wraz ze zgłoszonym przypadkiem 81.6 -> 82 i z komórką, która nie może zejść do zera;
- krok między lewymi krawędziami sąsiednich ikon jest **stały** — dla ośmiu kształtów list
  z shipowanego XML (plecak 4:3 i 16:9, pas, szybkie sloty, kosz, pionowy pas Clear Sky)
  przy sześciu skalach, w tym 1366x768, 1920x1080, 2560x1440 i 3440x1440;
- prawa krawędź ikony pokrywa się z lewą krawędzią sąsiada;
- tło siatki z `Draw` ląduje na tych samych pikselach co ikona, także na liście przewiniętej;
- `PickCell` trafia w narysowaną komórkę, 64 punkty na komórkę, na całej liście;
- przedmiot 2x1 ma dokładnie `2 x 1` komórki, a 5x2 dokładnie `5 x 2`;
- po zmianie skali `UpdateCellMetrics` zgłasza zmianę, `ReinitSize` poprawia rozmiar
  kontenera, a `RefreshItemsPos` przestawia przedmiot na nową siatkę; przy tej samej skali
  `UpdateCellMetrics` nie zgłasza nic;
- skala zerowa jest odrzucana, metryki zostają poprzednie, a `CellOffsetUI` dalej zwraca
  wartości skończone;
- lista `vertical_placement` zachowuje sztuczkę kwadratowych komórek;
- ostatni piksel pasa wskazuje ostatnią komórkę, a nie nieistniejącą;
- **zachowanie sprzed poprawki jest odtworzone**: przy tej samej skali stara geometria daje
  różne kroki między ikonami i niestykające się krawędzie. Bez tego sprawdzenia testy
  przechodziłyby też na siatce, która problemu nigdy nie miała.

**Granica testu.** Rasteryzacja ikony, czyli `CUIStaticItem::RenderInternal`, jest
w teście **odtworzona ręcznie**, nie skompilowana — skompilowanie jej wciąga `sPoly2D`,
`ClipPoly` i renderer. Test sprawdza więc model rasteryzatora, a nie sam rasteryzator.
To jest powód, dla którego próba w grze poniżej jest obowiązkowa, a nie kurtuazyjna.

Test wymaga Pythona 3 i g++ z ASan/UBSan; LeakSanitizer jest domyślnie wyłączony.

**Nie wykonano kompilacji na Windowsie ani próby w grze.** Po kompilacji sprawdź na kilku
rozdzielczościach, w tym takiej, przy której skala daje ułamek:

- plecak zapełniony po brzegi — żadnej szczeliny ani nachodzenia między ikonami;
- tło siatki pokrywa się z ikonami, także po przewinięciu;
- pas i szybkie sloty — odstępy równe, upuszczanie trafia w komórkę pod kursorem;
- broń 2x1 i kombinezon — rozmiar dokładnie wielokrotność komórki;
- podgląd upuszczania podświetla te same piksele, co komórka pod spodem;
- `vid_restart` przy otwartym ekwipunku — siatka ma się przeliczyć, nie rozjechać;
- proporcje 4:3, 16:9 i 21:9, bo `ui_core::get_xml_name` podstawia inny XML dla każdej;
- handel, przeszukanie zwłok, sloty wyposażenia i kosz.

## Użycie

Ustaw `PAKIET` na ścieżkę tego katalogu, a `IXRAY` na osobny, czysty checkout silnika
z nałożonymi już pakietami `inventory-drop-cell` i `inventory-drop-preview`.

```bash
python3 "$PAKIET/apply.py" --repo "$IXRAY"
python3 "$PAKIET/apply.py" --repo "$IXRAY" --apply
python3 "$IXRAY/tests/inventory-drop/test_cell_grid.py"
```

Pierwsze polecenie tylko sprawdza zgodność, w tym obecność obu zależności. Drugie nakłada
patch jako commit; rozpoznaje też poprawkę już obecną.

`patch.json` zawiera wersje, zależności i sumę SHA-256 eksportu. Przy aktualizacji IX-Ray
najpierw sprawdź patch na nowym checkoucie; zgodność z nowszym kodem nie jest
zagwarantowana. Zintegrowanej gałęzi nie należy patchować ponownie.
