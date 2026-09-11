# Ochrona strefowa: mechanika i skala UI

Zmiana z 11.09.2026, przygotowana na bazie `1bfd68afe`.

Dotyczy `burn`, `shock`, `chemical_burn`, `radiation` i `telepatic`.
`light_burn` używa tej samej ochrony co `burn`, zgodnie z ładowaniem odporności
artefaktów. Pozostałe typy obrażeń zachowują dotychczasową mechanikę.

## Obliczenia

Artefakty na pasie odejmują od siły trafienia sumę `immunity * condition`.
Wynik siły trafienia jest ograniczany od dołu do zera. Nie ma przekształcenia
wykładniczego ani limitu 0,99 dla tych pięciu ochron. Ujemna odporność zwiększa
siłę trafienia.

Kombinezon i hełm nadal działają przez istniejące `HitThroughArmor`:
`protection * condition * 0.1`. Ich funkcje mechaniki nie zostały zmienione.
`GetDefHitTypeProtection` zawiera już condition; UI nie mnoży przez stan ponownie.

`CActor::GetEquipmentProtection` zbiera ten sam wkład wyposażenia dla panelu
oraz pięciu nowych zmiennych wyrażeń `fltActor*ProtectionRatio`. Każda z tych
zmiennych zwraca sumę podzieloną przez `GetZoneMaxPower(EHitType)`.
Paski nie są już kwantowane do 31 segmentów. Clamp 0–1 dotyczy wyłącznie
graficznego wypełnienia; liczby i mechanika zachowują wartości powyżej 100%.

Efekty leków nie są składnikiem tych pasków wyposażenia. Ich działanie
mechaniczne oraz osobne wskazania pozostają bez zmian. Regeneracja,
pochłanianie promieniowania w czasie i tamowanie krwawienia są odrębnymi efektami.

## Karty przedmiotów i ulepszenia

- Artefakt: `immunity * condition / zone_max_power * 100%`.
- Kombinezon/hełm: `GetDefHitTypeProtection(hit_type) * 0.1 / zone_max_power * 100%`.
- Ulepszenie: `protection_delta * 0.1 / zone_max_power * 100%`, bez condition.

Opisy ulepszeń dla `prop_thermo`, `prop_electro`, `prop_chem`, `prop_radio`
i `prop_psy` czytają rzeczywisty parametr z sekcji efektu. Pole `value` nie
jest używane jako liczba ochrony. W ulepszeniach z kilkoma właściwościami każdy
typ otrzymuje własną deltę; przy kilku zainstalowanych ulepszeniach delty są
sumowane. Brak parametru nie powoduje powrotu do fikcyjnej wartości `value`.

Wszystkie karty korzystają z tych samych nazw ochron i wspólnego formatowania
procentów: do dwóch miejsc po przecinku, bez zbędnych zer, ze znakiem bonusu.
Małe niezerowe wartości są oznaczane `+<0,01%`/`-<0,01%`.
Nie korzystają ze starych mnożników XML 30/48. Porównanie kombinezonów i hełmów
uwzględnia stan obu przedmiotów. W tych pięciu wierszach pasek porównania jest
pod tekstem, aby zrobić miejsce na pełne nazwy i liczby.

Wąskie pola liczbowe na głównym panelu dodatku zachowują pełne procenty;
paski używają niezaokrąglonego wyniku, a tooltipy dokładności do 0,01%.
Sumy widocznych zaokrąglonych liczb mogą różnić się o końcową cyfrę.
100% oznacza ochronę równą maksymalnej mocy strefy, nie procent redukcji
każdego możliwego trafienia.

## Przykłady

- Kula ognista `0.04`, stan 100%, maksimum ognia `0.2`: +20%; trafienie
  `0.2` zostaje zredukowane przez artefakt do `0.16`.
- Ten sam artefakt przy stanie 1%: +0,2%; trafienie `0.2` zmniejsza do `0.1996`.
- Ulepszenie `burn_protection = 0.04`, maksimum `0.2`: +2%.
- Artefakty `0.04`, kombinezon `0.4` przy stanie 50%, hełm `0.2` przy stanie
  50%: wkłady +20%, +10%, +5%; suma +35%, czyli absolutna ochrona `0.07`.

## Weryfikacja

`tests/condition-ui/test_protection.py` kompiluje rzeczywiste funkcje źródłowe
z lekkimi atrapami usług silnika. Obejmuje naliczanie trafień wraz z niezmienionym
kodem pancerza, stany 0/1/50/100%, sumowanie, wartości ujemne, ponad 100%,
różne maksima stref, oba rodzaje porównania sprzętu, tooltip artefaktu,
wybór EHitType zamiast indeksu EInfluenceType, formatowanie, prawdziwe delty
ulepszeń i brak parametrów. Sprawdza także powiązania XML i teksty PL/EN/CZ/RU.

Uruchomienie z repozytorium silnika:

```sh
python3 tests/condition-ui/test_protection.py --mod-root /ścieżka/do/ixray-ui-params
```

Test korzysta z AddressSanitizer i UndefinedBehaviorSanitizer. W środowisku
z ptrace LeakSanitizer nie działa; wtedy uruchomienie wymaga
`ASAN_OPTIONS=detect_leaks=0`. Pozostałe kontrole pamięci pozostają aktywne.

Wynik: 6648 sprawdzeń nowej ochrony oraz kontrole XML i tekstów przeszły.
Przeszły też dotychczasowe testy regeneracji (1202 + 1202), jej tooltipów,
krwawienia i sześciu wariantów makr formatujących Windows.

To weryfikacja funkcji i powiązań danych. Pełna kompilacja Windows oraz
oględziny UI i próba obrażeń w grze pozostają do wykonania przed instalacją.
Zmiany źródeł silnika i dodatku należy wdrażać razem: nowe pliki panelu
wymagają nowych zmiennych wyrażeń dostarczanych przez skompilowany silnik.
