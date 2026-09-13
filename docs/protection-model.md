> Aktualizacja 13.09.2026: nowe tooltipy rozdzielają próg pancerza, artefakty,
> protection boostery i mnożnik odporności postaci. Bieżąca specyfikacja:
> [Tooltipy ochron środowiskowych](environmental-protection-tooltips.md).
> Dalszy opis obejmuje też wcześniejsze etapy panelu; nowe tooltipy nie sumują
> surowych współczynników artefaktów z progiem pancerza.

# Punkty ochrony środowiskowej

Poprawka prezentacji z 11.09.2026, oparta na `2b5637188`.

## Dlaczego poprzednie procenty wprowadzały w błąd

Dotychczasowa prezentacja dzieliła absolutną ochronę przez konfiguracyjne
`GetZoneMaxPower`. Ta wartość jest punktem odniesienia interfejsu, a nie siłą
każdego trafienia w konkretnej anomalii. 5% nie oznaczało redukcji obrażeń o 5%.

W aktywnym ixray-stcop-wp-outfits maksima wynoszą: ogień 0.2, elektryczność 0.8,
chemia 0.2, promieniowanie 0.03, psionika 0.1. Monolith scientific przy stanie
90.6% dawał `0.1 * 0.906 * 0.1 / 0.2 * 100 = 4.53%` ochrony termicznej oraz
`0.45 * 0.906 * 0.1 / 0.8 * 100 = 5.09625%` elektrycznej. Liczby z obu zrzutów
są spójne z tym przeliczeniem. Świt przy stanie około 92.6% daje około 3.01%.

Odejmowanie stałej siły trafienia powoduje silnie nieliniowy wpływ na pozostałe
obrażenia. Przykład obliczeniowy (nie pomiar anomalii ze zrzutu): trafienie
0.0135 po przejściu przez sprawny Świt ma siłę 0.007, a przez Monolit 0.0035.
Monolit ma o 53.85% większą ochronę termiczną, a w tym przykładzie pozostawia
połowę obrażeń Świtu. Czas życia zależy dodatkowo od częstotliwości i typu
trafień, stanu sprzętu, regeneracji oraz modyfikatorów postaci.

## Mechanika po przywróceniu wzoru IX-Ray — 13.09.2026

Wycofano środowiskową ścieżkę absolutnego odejmowania wprowadzoną przez
`2b5637188`. `CActor::HitArtefactsOnBelt` ponownie używa wzoru z `c2e7fc6f3`
(9.09.2025) dla wszystkich typów obrażeń, w tym burn, light_burn, shock,
chemical_burn, radiation i telepatic. Wybór dotyczy typu trafienia, nie źródła.

1. Sumowane są `immunity * condition` artefaktów na pasie.
2. Zerowa suma pozostawia siłę trafienia bez zmian. Pozostałe sumy są
   ograniczane do przedziału [-0.99, 0.99].
3. Dla S > 0 siła jest mnożona przez `1 - 1.5 * 0.9^(4/S)`;
   dla S < 0 przez `1 + 1.5 * 0.9^(4/abs(S))`.
4. Następnie kombinezon i hełm odejmują dla typów środowiskowych
   `GetDefHitTypeProtection(type) * 0.1`, ograniczając wynik od dołu do zera.
   Getter uwzględnia już stan sprzętu.
5. Dalej działają odpowiednie boostery, mnożniki odporności postaci i skutku
   trafienia. Radiacja zwiększa skażenie, zamiast od razu odejmować zdrowie.

Przykład: Kula ognista z S=0.04 pozostawia około 0.199992 z trafienia 0.2;
sprawny Monolit z burn_protection=0.1 odejmuje następnie 0.01. Artefakt nie
odejmuje już stałego 0.04. Ochrona pancerza jest progiem pochłaniania,
natomiast odporność postaci (`burn_immunity`) jest późniejszym mnożnikiem.
Skrypt OnBeforeHit może dodatkowo zmienić trafienie między tymi etapami.

Nie zmieniono parametrów artefaktów, konfiguracji anomalii, zużycia ani
interfejsu. Dostosowanie parametrów artefaktów jest osobnym przyszłym krokiem.

**Ograniczenie obecnego panelu:** nadal sumuje surowe współczynniki artefaktów
z absolutną ochroną sprzętu. Po przywróceniu wzoru te wielkości mają różne
znaczenie: suma punktów nie jest wspólnym progiem pochłaniania i nie pozwala
porównać ochrony z siłą źródła. Poniższe liczby UI opisują zachowane wskazania,
nie absorpcję mechaniczną. Odczyt naliczonych skutków trafienia nadal obserwuje
rzeczywiste rozliczenie i uwzględnia przywrócony wzór.

Aktywny mod ixray-t-anomaly ustawia dla rodziny zone_mine_thermal_* dodatkowe
trafienia o mocy 0.008 i blowout_time 7000 ms (ponadto awaking_time 100 ms,
accamulate_time 250 ms). CMosquitoBald::UpdateSecondaryHit zadaje trafienia
podczas aktywności strefy. To pasuje do opisu częstych małych obrażeń i
okresowego silniejszego uderzenia. Bez zapisu gry i pomiaru trafień nie można
ustalić dokładnego wariantu strefy ani wyprowadzić czasów 5/10 sekund.

## Nowa skala

`punkty = suma_wskazywana_przez_UI / zone_max_power * 1000`.

Dla samego pancerza suma jest ochroną absolutną; przy artefaktach zawiera
również ich surowe współczynniki, zgodnie z ograniczeniem opisanym powyżej.

`DisplayRatio` zwraca punkty / 100. Jest funkcją wyłącznie prezentacyjną;
oryginalna `Ratio` zachowuje dotychczasowe znaczenie. Mnożnik 10 względem
poprzedniego wypełnienia wykorzystuje zakres pasków dla obecnego zestawu
kombinezonów, zachowując ich proporcje i addytywność. To umowna jednostka,
nie procent redukcji obrażeń. Punktów różnych typów obrażeń nie porównujemy
jako jednakowych ilości energii.

| Wyposażenie, stan 100% | Termiczna |
|---|---:|
| Świt | 32.5 pkt |
| Monolit naukowy | 50 pkt |
| Kula ognista | +200 pkt |
| Monolit + Kula ognista | 250 pkt |

Pozostałe ochrony sprawnego Monolitu: elektryczna 56.25 pkt, chemiczna 65 pkt,
radiacyjna 66.67 pkt, psioniczna 60 pkt. Przy stanie ze zrzutu ochrona termiczna
wyniesie 45.3 pkt, a elektryczna 50.96 pkt.

100 pkt oznacza pełny pasek; liczby nie są ograniczane do 100.
W panelu ixray-ui-params przy wyniku powyżej 100 pkt za paskiem pojawia się
trójkąt. Przy dokładnie 100 pkt i poniżej jest ukryty. Znacznik reaguje na
aktualną sumę ochrony, także po zdjęciu artefaktu lub zużyciu wyposażenia. Wyposażenie
powyżej tej wartości nadal można porównywać po liczbach i podpowiedziach.
Wartości ujemne zachowują znak; ich pasek jest pusty.

## Interfejs

- Pięć ochron środowiskowych ma tę samą skalę we wszystkich miejscach.
- Tooltipy przedmiotów i ulepszeń: punkty z jednostką, do dwóch miejsc po
  przecinku, bez zbędnych zer. Bonusy mają znak plus.
- Ulepszenia nadal czytają rzeczywiste delty z sekcji efektów. Przykładowo
  burn_protection +0.01 daje +5 pkt, a +0.001 daje +0.5 pkt. Małe rzeczywiste
  ulepszenia pozostają małe; skala nie fałszuje ich względnej wartości.
- Wąska kolumna głównego panelu: całkowite punkty bez znaku procentu i bez
  jednostki; pełna jednostka i dokładność są w podpowiedzi wiersza. Dzięki
  temu pozostają dotychczasowe szerokości i tory pasków w teksturze.
- Podpowiedź panelu: suma oraz osobno kombinezon, hełm i artefakty na pasie,
  wyłącznie dla faktycznie założonego wyposażenia. Hełm zintegrowany nie ma
  osobnego wiersza. Artefakty pojawiają się tylko wtedy, gdy na pasie jest
  artefakt; obecny przedmiot z zerowym wkładem nadal jest widoczny. Tytuł
  oddziela od parametrów kropkowany separator. Uwzględnia aktualny stan sprzętu.
- Regeneracja, leki i ochrona fizyczna zachowują dotychczasowe wskazania.
- Teksty PL/EN/CZ/RU są dostępne w silniku i dodatku. Inne języki mają awaryjną
  jednostkę `pt`; rozbicie źródeł wymaga przetłumaczonych opisów.

## Wskaźnik siły źródła przed ochroną

Pięć pasków panelu dodatku zawiera cienką warstwę o tym samym położeniu,
szerokości i skali punktowej co ochrona. Szary pasek i liczba nadal oznaczają
wyposażenie. Cienka warstwa pokazuje szczyt zarejestrowanej siły przychodzących
trafień. Kolor identyfikuje typ: ogień pomarańczowy (255/160/80), chemia
jasnozielony (170/235/140), elektryka jasnoniebieski (130/210/255), psionika
jasnofioletowy (205/165/250), radiacja jasnożółty (245/230/120).
Kolor pozostaje stały niezależnie od przekroczenia ochrony.
Oba wypełnienia są ograniczane do pełnej długości; dokładna „Siła źródła”
w tooltipie może być większa niż 100. Dotychczasowy trójkąt dotyczy ochrony.
Warstwa ochrony aktualizuje się bez opóźnienia animacji, aby porównanie obu
warstw używało tego samego stanu wyposażenia.

Odczyt jest obserwacją trafień, nie prognozą pobliskiej anomalii: zaczyna się
przy pierwszym trafieniu danego typu, także całkowicie pochłoniętym.
CActor::Hit rejestruje HDS.damage() i typ przed zastosowaniem ochrony artefaktów,
pancerza i innymi modyfikatorami. Nie korzysta z wygaszanego maksimum
pobliskich stref na HUD-zie. Uwzględnia wszystkie źródła zadające dany typ
trafienia, także skrypty; burn i light_burn mają wspólny kanał termiczny.

Na aktora przypada stały bufor 16 koszyków, tworzonych nie częściej niż co
50 ms. W każdym jest osobny szczyt pięciu typów trafień. Zachowujemy próbki
młodsze niż 750 ms; słabsze trafienie w tym samym koszyku nie zastępuje
szczytu. To ograniczony pamięciowo odczyt szczytowy z rozdzielczością 50 ms,
nie suma sił kolejnych trafień i nie pomiar obrażeń na sekundę.
Przez 500 ms wskazanie ma pełną jasność, następnie do 750 ms maleje tylko
jego krycie. Długość nie jest sztucznie zmniejszana podczas wygaszania,
żeby nie sugerować wzrostu bezpieczeństwa. Zegar Device.dwTimeGlobal
zatrzymuje się z pauzą rozgrywki i respektuje tempo symulacji. Ponowny spawn
oraz reinit zerują bufor; aktualizacje zużycia sprzętu go nie zerują.

## Naliczone skutki ostatniego trafienia

Tylko tooltip pokazuje wynik ostatniego rozliczonego trafienia danego typu,
przez 3 s zegara silnika (pauza zatrzymuje odliczanie). To nie jest DPS ani
średnia; szczyt siły źródła z 0,75 s może pochodzić z innego trafienia.
Ogień, chemia i elektryka pokazują obrażenia zdrowia w procentach; psionika
pokazuje osobno zdrowie i zdrowie psychiczne. Radiacja pokazuje dawkę w kBq,
w tej samej skali co skażenie organizmu, bez dopisywania jej jako obrażeń HP.

CActorCondition::ConditionHit obserwuje różnicę akumulatorów zdrowia, zdrowia
psychicznego i radiacji przed oraz po CEntityCondition::ConditionHit. Wynik
uwzględnia rzeczywiście wykonane obliczenia artefaktów, pancerza, hełmu,
odporności, leków, mnożników i wcześniej wykonanych modyfikacji skryptowych.
Nie odtwarzamy wzoru w UI. Zerowy skutek zastępuje wcześniejsze obrażenia;
GodMode także zapisuje zero. Leczenie/ujemna szkoda nie jest obrażeniem.
Bufor jest lokalny dla stanu postaci, zerowany w load i reinit, nie zapisywany
w save. Burn i light_burn mają wspólny kanał, inne typy osobne.

Są to naliczone skutki bezpośredniego trafienia, przed późniejszym zastosowaniem
akumulatorów i ograniczeniem zasobów do ich zakresu. Nie jest to zmiana netto
paska zdrowia: pomiar nie obejmuje regeneracji, krwawienia, wtórnych obrażeń
od zgromadzonej radiacji ani zmian skryptowych poza rozliczeniem trafienia.
Liczba przy pasku nadal opisuje wyłącznie ochronę, nie obrażenia.

## Weryfikacja

`ASAN_OPTIONS=detect_leaks=0 python3 tests/condition-ui/test_protection.py --mod-root /ścieżka/do/ixray-ui-params`

Sprawdzenia skompilowanych funkcji, dodatkowe asercje rozbicia źródeł oraz
kontrole powiązań XML i tłumaczeń. Przypadki obejmują stany 0/1/50/100%, różne
maksima, ujemne wartości, wyniki ponad 100 pkt, brak wyposażenia, rzeczywiste
parametry Monolitu, Świtu i Kuli ognistej oraz stan ze zrzutów. Obliczenia obrażeń zachowano; dopisano obserwację przychodzących i rozliczonych trafień. Przeszły również istniejące testy regeneracji,
podpowiedzi regeneracji i sześć wariantów kompilacji formatowania.

Pełna kompilacja gry Windows, oględziny UI i ponowienie próby w anomalii
pozostają do wykonania. Silnik i dodatek należy wdrażać razem.

`python3 tests/condition-ui/test_environmental_damage.py` sprawdza produkcyjne
rozliczenie obrażeń i jego obserwator, niezależne kanały, zero po pochłonięciu,
GodMode, leki, mnożniki, regenerację w akumulatorach, pauzę i wygaśnięcie.

Weryfikacja przywrócenia z 13.09.2026 obejmuje wzór wykładniczy dla sześciu
typów środowiskowych, wartości dodatnie i ujemne, limit 0.99, sumowanie pasa,
stan artefaktów oraz kolejność: mnożnik artefaktów, potem odejmowanie pancerza.
Testy UI zachowują dotychczasowe wskazania; nie potwierdzają ich równoważności
z progiem pochłaniania po zmianie mechaniki.

`ASAN_OPTIONS=detect_leaks=0 python3 tests/condition-ui/test_artefact_damage.py`
przechodzi 43219 sprawdzeń produkcyjnych funkcji artefaktów, kombinezonu i hełmu
oraz kontroli kolejności etapów. `test_environmental_damage.py` przechodzi
25 sprawdzeń i asercje historii. Funkcja artefaktów jest identyczna z wersją
`1bfd68afe` sprzed lokalnej poprawki, z pominięciem białych znaków.

Szerszy `test_protection.py` obecnie nie kompiluje atrap UI po późniejszych
zmianach tooltipów (m.in. FormatProtectionPointsPlain, GetMaxBoneArmor,
GetEnvironmentalDamageRate). Ten sam problem odtworzono na kopii testu
sprzed przywrócenia wzoru. Oczekiwania mechaniki w nim zaktualizowano, ale
nie zgłaszamy całego zestawu UI jako zaliczonego. Weryfikacja funkcji nie
zastępuje kompilacji i instalacji nowego silnika Windows ani próby w grze.

## Ochrona bojowa: rozszarpanie / uderzenie / wybuch / balistyka (13.09.2026)

Cztery wiersze bojowe panelu (`wound_sensor`, `main_sensor`, `sleeping_state`,
`fire_wound_sensor`) liczy `ui_actor_state_wnd::UpdateCombatProtection`
(UIActorStateInfo.cpp). Liczba, pasek, trójkąt i podpowiedź powstają w C++ —
wiersze nie mają już wyrażeń XML (usunięto `fltActorOutfit*`/`fltZoneMaxPower*`
z panelu). Pokazujemy **wyłącznie parametry ochronne**; brak siły źródła,
mnożnika obrażeń ponad próg i odczytu ostatniego trafienia (świadomie — panel
ochrony nie jest oglądany w trakcie walki wręcz).

**Rozszarpanie/uderzenie/wybuch (model progowy, jednostka „pkt", skala ×100).**
Silnik odejmuje `immunity×condition` kombinezonu i hełmu od siły trafienia
(`HitThroughArmor`, mnożnik `one=1.0` dla typów niestrefowych), a artefakty na
pasie mnożą trafienie przez `(1−f)`, gdzie `f=1.5·0.9^(4/Σ immunity×cond)`
(`HitArtefactsOnBelt`). Panel pokazuje:

- `Próg ochrony` = suma `GetDefHitTypeProtection` kombinezonu i hełmu;
- `Efektywny próg` = `Próg / (1−f)` — maksymalne trafienie pochłonięte w całości;
- rozbicie na kombinezon/hełm (gdy >0) i `Wpływ artefaktów` = `f` z liniami
  surowego `*_immunity` per artefakt (brak takich artefaktów w bazowej grze);
- statyczny `Skutek trafienia` (rozszarpanie: rany i krwawienie; uderzenie: bez
  krwawienia; wybuch: możliwe rany).

Pasek = Efektywny próg (pełny przy 1.00 = 100 pkt), trójkąt gdy >100 pkt.
`GetZoneMaxPower` nie jest używane (skala `Ratio` ÷1.0, nie środowiskowa ×10).

**Balistyka (model absorpcji, jednostka „%").** W SP `HitThroughArmor` przy
zatrzymaniu pocisku (`ap ≤ pancerz_kości`) mnoży trafienie przez
`hit_fraction_actor` (część przechodząca; hełm = 1), a przy przebiciu nie
redukuje. Następnie działa mnożnik trudności `GetHitImmunity(eHitTypeFireWound)`
(actor_immunities_<diff>; efekty czasowe `m_fBoost*Immunity` = 0 w bazowej grze,
pominięte). Tooltip rozbija to na dwa scenariusze:

- po zatrzymaniu: `Absorpcja pancerza` = `1−hit_fraction`, `Dodatkowa redukcja
  (trudność)` = `hit_fraction·(1−mnożnik)`, `Efektywna ochrona` = ich suma
  = `1 − hit_fraction·mnożnik`; krwawienie: nie;
- przy przebiciu: `Efektywna ochrona` = `1 − mnożnik`; krwawienie: możliwe.

Przykład cs_heavy (`hit_fraction_actor=0.45`): Absorpcja 55%; Dodatkowa redukcja
Novice 38,25 / Stalker 22,5 / Weteran 11,25 / Master 0%; Efektywna 93/77/66/55%.
Pasek = Efektywna ochrona (po zatrzymaniu); bez trójkąta (≤100%).

Usunięto dawną, niewidoczną ścieżkę `fwou_value`/`woun_value` z pancerza kości w
`UpdateActorInfo`. Teksty w dodatku: `configs/text/*/zz_uiparams_panel.xml`
(klucze `ui_uip_prot_*`). Formuły zweryfikowano ręcznie na przykładzie cs_heavy;
`f` artefaktów pokrywa `test_artefact_damage.py`. Kompilacja silnika, oględziny
tooltipów i próba w grze (mutant / seria / granat / postrzał w pancerz i przez
pancerz, zmiana poziomu trudności) pozostają do wykonania po stronie użytkownika.
