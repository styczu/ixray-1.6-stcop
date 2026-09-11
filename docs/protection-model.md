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

## Mechanika pozostaje bez zmian względem 2b5637188

Dla burn/light_burn/shock/chemical_burn/radiation/telepatic:

- artefakty odejmują sumę `immunity * condition`;
- kombinezon i hełm odejmują `GetDefHitTypeProtection(type) * 0.1`;
- `GetDefHitTypeProtection` zawiera już stan przedmiotu;
- kolejne etapy obrażeń ograniczają wynik od dołu do zera.

Poprzednia zmiana 2b5637188 wprowadziła absolutne odejmowanie przez artefakty.
Ta poprawka go zachowuje. Nie zmienia konfiguracji wyposażenia ani anomalii.
Sprawny Monolit odejmuje termicznie 0.01, Świt 0.0065, Kula ognista 0.04.
Kula wnosi więc czterokrotność ochrony tego kombinezonu.

Aktywny mod ixray-t-anomaly ustawia dla rodziny zone_mine_thermal_* dodatkowe
trafienia o mocy 0.008 i blowout_time 7000 ms (ponadto awaking_time 100 ms,
accamulate_time 250 ms). CMosquitoBald::UpdateSecondaryHit zadaje trafienia
podczas aktywności strefy. To pasuje do opisu częstych małych obrażeń i
okresowego silniejszego uderzenia. Bez zapisu gry i pomiaru trafień nie można
ustalić dokładnego wariantu strefy ani wyprowadzić czasów 5/10 sekund.

## Nowa skala

`punkty = ochrona_absolutna / zone_max_power * 1000`.

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
  także zerowe składniki. Uwzględnia aktualny stan sprzętu.
- Regeneracja, leki i ochrona fizyczna zachowują dotychczasowe wskazania.
- Teksty PL/EN/CZ/RU są dostępne w silniku i dodatku. Inne języki mają awaryjną
  jednostkę `pt`; rozbicie źródeł wymaga przetłumaczonych opisów.

## Weryfikacja

`ASAN_OPTIONS=detect_leaks=0 python3 tests/condition-ui/test_protection.py --mod-root /ścieżka/do/ixray-ui-params`

6738 sprawdzeń skompilowanych funkcji, dodatkowe asercje rozbicia źródeł oraz
kontrole powiązań XML i tłumaczeń. Przypadki obejmują stany 0/1/50/100%, różne
maksima, ujemne wartości, wyniki ponad 100 pkt, brak wyposażenia, rzeczywiste
parametry Monolitu, Świtu i Kuli ognistej oraz stan ze zrzutów. Nie zmieniono
źródeł obliczania trafień. Przeszły również istniejące testy regeneracji,
podpowiedzi regeneracji i sześć wariantów kompilacji formatowania.

Pełna kompilacja gry Windows, oględziny UI i ponowienie próby w anomalii
pozostają do wykonania. Silnik i dodatek należy wdrażać razem.
