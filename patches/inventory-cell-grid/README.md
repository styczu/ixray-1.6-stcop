# Stały rozmiar komórki ekwipunku w pełnych pikselach ekranu

Po przeskalowaniu UI komórka wypadała na ekranie ułamkowo, na przykład 81.6 px.
W efekcie w plecaku jedne sąsiednie ikony stykały się poprawnie, inne miały piksel
szczeliny, a jeszcze inne lekko na siebie nachodziły. Po poprawce każda lista ma jeden,
stały, całkowity rozmiar komórki, całą geometrię liczy właśnie z niego, a ikona
rysuje się na pełnych pikselach obiema krawędziami.

Dodatkowo nowy atrybut XML `screen_cell_size` pozwala podać rozmiar kwadratowej komórki
wprost w pikselach ekranu, niezależnie od proporcji, w których `cell_width`/`cell_height`
dają prostokąt (33x41 przy skali 2.5/1.875 to 83x77 px).

Na tym opierają się dwie poprawki paska przewijania list. Pasek pokazuje się tylko wtedy,
gdy ma niepusty zakres, bo zaokrąglenie komórki stawiało martwy pasek przy szybkich
slotach i pasie. Lista może też wskazać profil paska atrybutem `scroll_profile`,
a nieistniejący profil wraca do `default`, zamiast wywalać grę.

Patch to cztery commity w jednym pliku (`git am` nakłada je po kolei). **Nie zmienia
`gamedata`**: wartości atrybutów dla ekwipunku dowozi dodatek `ixray-hd-icons`.

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

Sam stały krok nie wystarczył. Nawet przy całkowitym `skala * rozmiar_komórki` prawa
krawędź ikony dalej była liczona jako `podłoga(LT) + przeskalowany_rozmiar`, czyli
wychodziła ułamkowo, podczas gdy lewa krawędź sąsiada była już zaokrąglona. Dlatego
potrzebne są **obie** rzeczy: stały całkowity krok i zaokrąglanie obu rogów.

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

### Kwadratowa komórka: `screen_cell_size`

Opcjonalny atrybut na węźle listy:

```xml
<dragdrop_bag ... cell_width="33" cell_height="41" screen_cell_size="75" .../>
```

Gdy jest dodatni, to on jest całą geometrią listy: `m_cellSizeScreen` to `(px, px)`,
a wartości dla `CUIWindow` biorą się z niego, osobno na każdej osi:

```
m_cellSize.x = screen_cell_size / skala_x
m_cellSize.y = screen_cell_size / skala_y
```

`cell_width` i `cell_height` przestają wtedy decydować o rozmiarze. Bez atrybutu nic się
nie zmienia — rozmiar dalej wychodzi z nich przez `round(wartość * skala)`, więc wszystkie
istniejące layouty działają jak dotąd.

Patch nie ustawia atrybutu nigdzie w `gamedata`. Wartość `75` dla sześciu ciągłych siatek
daje dodatek `ixray-hd-icons` w swoim `configs/ui/actor_menu_16.xml`: `dragdrop_bag`,
`dragdrop_actor_trade`, `dragdrop_actor_trade_bag`, `dragdrop_partner_trade`,
`dragdrop_partner_bag`, `dragdrop_deadbody_bag`. Kosz, pas, szybkie sloty i sloty
wyposażenia zostają tam bez atrybutu, na dotychczasowym zachowaniu.

### Obie krawędzie ikony na pełnym pikselu

`CUIStaticItem` dostaje flagę `flSnapToScreenPixels`, domyślnie wyłączoną. Gdy jest
włączona, `RenderInternal` bierze oba rogi z pozycji bezwzględnej i zaokrągla oba do
najbliższego piksela nowym `ui_core::SnapPixel`:

```
LT = snap(skala * pozycja)
RB = snap(skala * (pozycja + rozmiar))
```

Krawędź wspólna dwóch sąsiadów to wtedy ta sama wartość zaokrąglona tak samo z obu stron,
więc `prawa(N) == lewa(N+1)` i `dół(N) == góra(pod N)` z definicji, a nie z dokładności
arytmetyki. Flagę włączają wyłącznie `CUICellItem` i ikony tła szybkich slotów — reszta
UI rysuje się bez żadnej zmiany. Wierzchołki siatki i podglądu upuszczania przeszły na to
samo zaokrąglenie (`snap_grid_px`), żeby tło dalej leżało na tych samych pikselach.

Zaokrąglanie do najbliższego piksela samo pochłania błąd `float` na granicy całkowitej —
tam, gdzie zaokrąglanie w dół zamieniało 1e-4 w cały piksel. Dlatego `kCellPixelBias`
z pierwszej wersji tego pakietu **został usunięty**; nie ma już czego korygować.

### Pasek przewijania tylko z niepustym zakresem

Po przejściu komórki na pełne piksele przy szybkich slotach i przy pasie pojawiał się
pionowy pasek, choć to jeden rząd. Winny jest round-trip jednostek: `cell_height="41"`
przy skali pionowej 1.875 to 77 px, a z powrotem 77/1.875 = 41.0667 jednostki. Kontener
listy jednorzędowej wychodzi więc o ułamek wyższy niż okno `height="41"`, a
`CUIDragDropListEx::ReinitScroll` pokazywał pasek już przy `dh > 0`.

Zakres paska to jednak `iFloor(dh)`, a pozycja przewinięcia jest całkowita, więc dla
`0 < dh < 1` pasek miał zakres `(0, 0)` i nie mógł przesunąć niczego. Teraz o widoczności
decyduje ten sam zakres, który i tak trafia do `SetRange`. Wartości zakresu są identyczne
jak dotąd, zmienia się tylko widoczność i tylko dla `0 < dh < 1`. Prawdziwe przepełnienie to
co najmniej jeden rząd, a artefakt zaokrąglenia nie przekracza 0.5/skala jednostki.
`always_show_scroll="1"` (plecak, handel, zwłoki) wygrywa jak dotąd.

### Profil paska: `scroll_profile`

`CUIXmlInitGame::InitDragDropListEx` czyta opcjonalny atrybut `scroll_profile` i przekazuje
go przez `InitDragDropList` do `CUIScrollBar::InitScrollBar`, którego czwarty parametr
listy dotąd zawsze wypełniały wartością `"default"`. To ta sama nazwa atrybutu, której używa
`CUIXmlInit::InitScrollView`. Bez atrybutu lista dostaje `"default"`, czyli wszystko po
staremu. Dodatek `ixray-hd-icons` wskazuje tak profil `<inventory>` z paskiem szerokości 6
w swoim `scroll_bar{,_16}.xml`.

### Brakujący profil wraca do `default`

`CUIScrollBar::InitScrollBar` (`src/xrUI/Widgets/UIScrollBar.cpp`) nie sprawdzał, czy profil
istnieje. Przy brakującym czytał wysokość zero, a zaraz potem `CUIXmlInit::Init3tButton`
kończył się `R_ASSERT4` na brakującym `profil:up_arrow`. Ten assert znika wyłącznie w configu
Shipping, więc w buildach z CI gra wywala się przy otwieraniu okna. Łatwo w to trafić:
wystarczy, że `actor_menu*.xml` wskazuje profil, a użyty `scroll_bar.xml` go nie ma —
na przykład gdy inny dodatek podmienia `scroll_bar.xml` własną kopią. Teraz brak węzła profilu
daje wpis `! [...]: scroll bar profile [...] not found, falling back to [default]` i cały
pasek (szerokość, strzałki, tło, suwak) buduje się z `default`. Dla istniejących profili nie
zmienia się nic.

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
do pół piksela na komórkę. W `actor_menu_16.xml` z `gamedata` plecak ma 7 komórek po 33 (231 jednostek)
w oknie `width="247"`, więc zapas jest duży; pas ma już dziś 241 przy `width="240"`,
a szybkie sloty 228 przy `227`. `GetClientArea` obcina nożycami, więc w najgorszym razie
ostatnia kolumna traci kilka pikseli po prawej.

**`screen_cell_size` jest fizyczny, więc w jednostkach bazowych rośnie przy niższej
rozdzielczości — i to trzeba mieć na uwadze przy doborze wartości.** Konfiguracja
z `ixray-hd-icons`: `75`, osiem kolumn, okno plecaka `width="247" height="574"` i pasek
szerokości 6, czyli 241 jednostek obszaru klienta w poziomie:

| rozdzielczość | skala | komórka w jedn. bazowych | siatka 8x14 | mieści się? |
|---|---|---|---|---|
| 2560x1440 | 2.5 / 1.875 | 30.0 x 40.0 | 240 x 560 | tak, na styk (1 jednostka zapasu) |
| 1920x1080 | 1.875 / 1.40625 | 40.0 x 53.3 | 320 x 746 | **nie** — mieści się 6 kolumn, 7. i 8. obcięte, plecak przewija |

Czyli `75` przy ośmiu kolumnach jest dobrane pod 1440p. Na 1080p ta sama wartość wyjdzie
poza okno w poziomie (nożyce utną ostatnie kolumny) i doda przewijanie w pionie. Jeżeli ma działać na wielu
rozdzielczościach, wartość trzeba dobrać do najmniejszej z nich albo rozszerzyć okna
w XML. Listy bez atrybutu nie mają tego problemu, bo skalują się razem z oknem.

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

Wyglądu i skali ekwipunku poza zaokrągleniem i znikającym martwym paskiem przewijania,
dopóki XML nie użyje `screen_cell_size` ani `scroll_profile`: `cell_width`/`cell_height` z XML znaczą to
samo co dotąd, wejście `SetCellSize` dalej bierze `Ivector2`, nie ma nowych opcji ani zmian
w `gamedata`. Nie zmienia się też rozmiar ikony wynikający z `inv_scale` — `ScaleIcon`
trafia wyłącznie do prostokąta tekstury, co opisuje README pakietu `inventory-drop-cell`.

Po stronie gry: `UIDragDropListEx.{h,cpp}`, `UICellItem.cpp`,
`UIDragDropReferenceList.{h,cpp}` i `UIHelperGame.cpp` (odczyt `screen_cell_size`
i `scroll_profile`). Po stronie `xrUI`: `ui_base.{h,cpp}` (`SnapPixel`),
`UIStaticItem.{h,cpp}` (flaga i gałąź w `RenderInternal`) — obie zmiany są opcjonalne
i nieaktywne, dopóki ktoś nie włączy flagi — oraz `Widgets/UIScrollBar.cpp` (powrót do
profilu `default`). Żadnych plików w `gamedata`. Test `tests/inventory-drop/test_cell_grid.py`;
dwa istniejące testy zmieniają się razem z typami i modelem rasteryzacji, które sprawdzają.

## Gałąź źródłowa

Patch to eksport commitów `feature/inventory-drop-preview..feature/inventory-cell-grid`
z pominięciem commitu z pakietami na czubku. Dokładne SHA są w `patch.json`
(`original_commits`), a te same zmiany w `build/tmz` w `integrated_commits`. Tamte
dwa commity niosły jeszcze dodanie i cofnięcie `screen_cell_size` w `gamedata`, więc
ich patch-id różni się od odpowiedników na gałęzi źródłowej.

## Sprawdzone

Stan z 15 września 2026, z odbudowy łańcucha gałęzi inventory. Patch (4 commity) nakłada
się na czysty upstream `6c793faee008d83f86cec2429d39d7aa39b5bc66` z nałożonymi
`inventory-drop-cell` i `inventory-drop-preview`. Po nałożeniu całe drzewo jest identyczne
z gałęzią źródłową, a pliki dotknięte przez rodzinę inventory także z `build/tmz`.
Sprawdzono też, że `apply.py` odmawia bez obu zależności i bez drugiej z nich, a na
repozytorium z już nałożoną poprawką zgłasza jej obecność zamiast nakładać ją drugi raz.
Wszystkie trzy testy `tests/inventory-drop/` przechodzą po każdym z trzech nałożeń i na
każdym commicie gałęzi źródłowej.

Dołączony test przechodzi w świeżo zapatchowanym checkoucie. Kompiluje produkcyjne
`screen_cell_len`, `snap_grid_px`, `UpdateCellMetrics`, `SetScreenCellSize`, `CellOffsetUI`,
`SetItemGeometry`, `RefreshItemsPos`, `ReinitSize`, `ReinitScroll`, `PickCell`, `ValidCell`,
`GetCellAt`, `TopVisibleCell`, `GetTexUVLT`, `GetCellsInRange`, `Draw` i `DrawDropPreview`,
i sprawdza:

- rozmiar efektywny jest całkowity i równy `round(wartość * skala)`, osobno na każdej osi,
  wraz ze zgłoszonym przypadkiem 81.6 -> 82 i z komórką, która nie może zejść do zera;
- krok między lewymi krawędziami sąsiednich ikon jest **stały** — dla dziesięciu kształtów
  list (z shipowanego XML: plecak 4:3 i 16:9, pas, szybkie sloty, kosz, pionowy pas Clear Sky;
  do tego dwie listy z wymuszonym `screen_cell_size`) przy sześciu skalach, w tym 1366x768,
  1920x1080, 2560x1440 i 3440x1440;
- prawa krawędź ikony pokrywa się z lewą krawędzią sąsiada;
- tło siatki z `Draw` ląduje na tych samych pikselach co ikona, także na liście przewiniętej;
- `PickCell` trafia w narysowaną komórkę, 64 punkty na komórkę, na całej liście;
- przedmiot 2x1 ma dokładnie `2 x 1` komórki, a 5x2 dokładnie `5 x 2`;
- po zmianie skali `UpdateCellMetrics` zgłasza zmianę, `ReinitSize` poprawia rozmiar
  kontenera, a `RefreshItemsPos` przestawia przedmiot na nową siatkę; przy tej samej skali
  `UpdateCellMetrics` nie zgłasza nic;
- skala zerowa jest odrzucana, metryki zostają poprzednie, a `CellOffsetUI` dalej zwraca
  wartości skończone;
- `screen_cell_size` daje komórkę **kwadratową i dokładnie zadaną** przy pięciu skalach,
  a `m_cellSize` to ta wartość podzielona przez skalę osobno na każdej osi;
- przy `screen_cell_size="75"`: 1x1 to 75x75, 2x1 to 150x75, 5x2 to 375x150, a trzy pełne
  ikony 1x1 obok siebie zajmują dokładnie 225 px, tak samo trzy w kolumnie — z równością
  `prawa(N) == lewa(N+1)`, nie z tolerancją;
- bez atrybutu wyliczenie zostaje stare (33x41 przy skali 2.5/1.875 to nadal 83x77),
  a ustawienie i wyzerowanie atrybutu wraca dokładnie do tego;
- lista `vertical_placement` zachowuje sztuczkę kwadratowych komórek;
- ostatni piksel pasa wskazuje ostatnią komórkę, a nie nieistniejącą;
- `ReinitScroll`: szybkie sloty i pas nie stawiają paska mimo dodatniego `dh`, plecak przy
  1080p dalej dostaje zakres 172 i przewija się, a `always_show_scroll` wygrywa przy
  `dh <= 0`; bez poprawki w silniku pierwsza z tych asercji pada;
- osiem kolumn po 75 px mieści się w obszarze klienta, który zostawia pasek szerokości 6;
- **zachowanie sprzed poprawki jest odtworzone**: przy tej samej skali stara geometria daje
  różne kroki między ikonami i niestykające się krawędzie. Bez tego sprawdzenia testy
  przechodziłyby też na siatce, która problemu nigdy nie miała.

**Granica testu.** Rasteryzacja ikony, czyli `CUIStaticItem::RenderInternal`, jest
w teście **odtworzona ręcznie**, nie skompilowana — skompilowanie jej wciąga `sPoly2D`,
`ClipPoly` i renderer. Test sprawdza więc model rasteryzatora, a nie sam rasteryzator.
To jest powód, dla którego próba w grze poniżej jest obowiązkowa, a nie kurtuazyjna.
Sprawdzono też mutacjami, że test nie jest pusty: zamiana `snap_grid_px` z powrotem na
zaokrąglanie w dół, zignorowanie `screen_cell_size` i odwrócenie dzielenia przez skalę
w `CellOffsetUI` — każda z nich wywala test.

Powrotu do profilu `default` w `UIScrollBar.cpp` **nie pokrywa żaden test** — wymagałby
`CUIXml` i widgetów strzałek.

Test wymaga Pythona 3 i g++ z ASan/UBSan; LeakSanitizer jest domyślnie wyłączony.

Kompilacja na Windowsie przeszła na samym łańcuchu źródłowym, bez zmian panelu, fontów
i CI. Na `02915a7da` (ostatni commit kodu gałęzi `feature/inventory-cell-grid`, wypchnięty
do CI jako tymczasowa `rebuild/inventory-cell-grid`) zielone są oba workflow: `Build engine`
w RelWithDebInfo (run `34948207427`) oraz `Non-Unity build` w Debug, RelWithDebInfo
i Release (run `34948207403`). Workflow upstreamu na czystym `default` nie buduje
`Build engine` w Release. W Release silnik z tym kodem zbudował się na `7fcb532af`, w dawnej
gałęzi z konfiguracją CI z `build/tmz`. Konfiguracja Debug ma tu znaczenie: `FindSimilar`
w zmienianym `UIDragDropListEx.cpp` ma w środku gałąź `#ifdef DEBUG`, a kompiluje ją wyłącznie
`Non-Unity build` w Debug.

`Build engine` wymagał ponowienia: pierwsze podejście padło w konfiguracji CMake, zanim
doszło do kompilacji, na braku nagłówków Discord GameSDK. `cmake/github.cmake` pobiera je
zipem tylko `if(NOT EXISTS`, więc obcięty zip przywrócony z cache'a NuGet blokuje i
pobranie, i rozpakowanie. Nie ma to związku z tym pakietem — w logu nie ma ani jednego
z jego plików — a ponowienie przeszło. Jeśli trafisz na to samo, skasuj cache
`Engine-NuGet-*` i powtórz run.

Pierwszą wersję sprawdzono w grze i to ona wykazała oba problemy, które ten pakiet teraz
rozwiązuje: komórki 83x77 zamiast kwadratowych i szczeliny jednego piksela między pełnymi
ikonami.

**Obecny kod sprawdzono w grze w buildach integracyjnych, nie jako sam pakiet.** Build
z `7fcb532af` (dawna gałąź `feature/inventory-cell-grid`, ten sam kod plus zmiany panelu)
sprawdzono przy 2560x1440 z dodatkiem `ixray-hd-icons`: osiem kolumn, cieńszy pasek,
brak martwych pasków. 15 września 2026 na `236edf3a7` z `build/tmz` ekwipunek wyglądał
poprawnie. Nie sprawdzono:
- innych rozdzielczości i proporcji;
- `vid_restart`;
- zachowania przy brakującym profilu paska, czyli powrotu do `default`;
- buildu z samych pakietów nałożonych na czysty upstream.

Sprawdź na kilku rozdzielczościach, w tym takiej, przy której skala daje ułamek:

- plecak zapełniony po brzegi — żadnej szczeliny ani nachodzenia między ikonami;
- komórki są kwadratowe: trzy pełne ikony 1x1 obok siebie zajmują dokładnie 3 x 75 px;
- na 1080p sprawdź prawą krawędź plecaka i przewijanie, patrz tabelka wyżej;
- tło siatki pokrywa się z ikonami, także po przewinięciu;
- pas i szybkie sloty — odstępy równe, upuszczanie trafia w komórkę pod kursorem;
- broń 2x1 i kombinezon — rozmiar dokładnie wielokrotność komórki;
- podgląd upuszczania podświetla te same piksele, co komórka pod spodem;
- `vid_restart` przy otwartym ekwipunku — siatka ma się przeliczyć, nie rozjechać;
- proporcje 4:3, 16:9 i 21:9, bo `ui_core::get_xml_name` podstawia inny XML dla każdej;
- handel, przeszukanie zwłok, sloty wyposażenia i kosz;
- przy szybkich slotach i pasie nie ma paska przewijania, a plecak, handel i zwłoki go mają;
- lista wskazująca przez `scroll_profile` profil, którego nie ma w `scroll_bar.xml`: okno
  ma się otworzyć z paskiem w wyglądzie `default`, a w logu ma być wpis
  `scroll bar profile [...] not found`.

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
