# Aktualny układ tooltipów ochrony — 13.09.2026

Tooltipy środowiskowe pokazują tytuł, progi i składowe ochrony oraz wpływ
artefaktów. Po separatorze, gdy historia źródła jest aktywna, są tylko:
`Siła oddziaływania: X,XX pkt` i `Otrzymywane obrażenia: X,XX%`.
Siła to ten sam szczyt z 750 ms co pasek. Nie ma wierszy Bieżące/Szczytowe,
nagłówka ODDZIAŁYWANIE ani sekcji Po przekroczeniu progu.

Obrażenia oznaczają naliczony skutek ostatniego trafienia, po ochronie,
przed regeneracją i końcowym ograniczeniem zasobu. To nie jest DPS ani
procent przepuszczanego trafienia. Dla psioniki pokazują ubytek psychiki;
dla radiacji druga linia brzmi `Otrzymana dawka: X,XX rad` i pokazuje dawkę
ostatniego trafienia zamiast całkowitego skażenia organizmu.
Historia źródła i ostatnie trafienie mają niezależne okna: szczyt siły może
pochodzić z wcześniejszego impulsu niż pokazane obrażenia. Sekcja znika
po wygaśnięciu historii źródła (750 ms bez trafień).

Nie ma wyrównywania do kolumn. Składowe mają krótkie wcięcie jak w tooltipie
balistycznym i ciemniejszy kolor całego wiersza. Liczby w szczegółach ochrony
środowiskowej, mechanicznej i balistycznej zachowują dwa miejsca dziesiętne,
np. 25,00; bardzo małe niezerowe wartości nadal mają zapis <0,01.
Główne liczby panelu oraz mechanika obrażeń nie zmieniają się.

Poniżej dokumentacja modelu i historyczne przykłady wcześniejszego układu.

---

# Tooltipy ochron środowiskowych — 13.09.2026

Zmiana informacyjna, przygotowana na lokalnym silniku z przywróconym wzorem IX-Ray
w `HitArtefactsOnBelt`. Nie zmienia konfiguracji artefaktów, poziomów trudności,
mechaniki trafienia ani metody pomiaru aktualnego oddziaływania. Zachowano równoległe
zmiany ochron bojowych w UIActorStateInfo i XML panelu.

## Źródła i kolejność

Sprawdzono `CActor::HitArtefactsOnBelt`, `CEntityCondition::HitOutfitEffect`,
`CCustomOutfit::HitThroughArmor`, `CHelmet::HitThroughArmor`,
`CEntityCondition::ConditionHit`, `CActor::OnDifficultyChanged`,
`ProtectionValues.h`, `UpdateProtectionHints`, delegaty panelu oraz XML i tłumaczenia dodatku.

Wartości informacji:

- `O = outfit->GetDefHitTypeProtection(type) * 0.1` albo 0 bez kombinezonu.
- `H = helmet->GetDefHitTypeProtection(type) * 0.1` albo 0 bez hełmu.
- `P = O + H`. Getter sprzętu już uwzględnia jego stan, więc nie mnożymy ponownie.
- `B = GetEnvironmentalProtectionBoost(type)`; getter zwraca bieżące pole
  protection, a nie współczynnik z boostera immunity.
- `A = actor->HitArtefactsOnBelt(1.0f, type)`. UI nie kopiuje wzoru artefaktów.
- `D = GetEnvironmentalHitMultiplier(type)`; bieżąca odporność aktora minus
  bieżący immunity booster, bez dodatkowego ograniczenia do 0–1.
- `Wpływ artefaktów = (1 - A) * 100%`.
- Dla zwykłych nieujemnych warstw `Efektywny próg = (P + B) / A`.

`Próg ochrony` pokazuje P (sam kombinezon i hełm). Wiersz `Efekt czasowy` ujawnia B,
które dodatkowo wchodzi do efektywnego progu. Surowe współczynniki artefaktów nie
są dodawane do żadnego z tych progów. Sumujemy je tylko wewnątrz funkcji silnika.

Mechanika, odzwierciedlana przez informację:

```
S = clamp(sum(artefact_immunity[type] * artefact_condition), -0.99, 0.99)
A = 1                                 gdy S == 0
A = 1 - 1.5 * pow(0.9, 4/S)            gdy S > 0
A = 1 + 1.5 * pow(0.9, 4/abs(S))       gdy S < 0
H1 = H0 * A
H2a = max(0, H1 - O)
H2b = max(0, H2a - H)
H3 = max(0, H2b - B)                   dla chemii/radiacji/psioniki
H3 = H2b                              dla burn/light_burn/shock
H4 = H3 * D
```

Nie ma ochrony przez przedmiot, którego nie założono. Ponieważ początkowy hit jest
nieujemny i A > 0, pominięcie nieobecnej zerowej warstwy daje ten sam wynik.
Artefakty mają limit sumy ±0.99 i nadal uwzględniają swój stan.

## Różnice między typami

| Typ | P | B — pole aktualnej ochrony czasowej | D | Dalszy skutek |
|---|---|---|---|---|
| burn | O + H dla burn | 0 | GetHitImmunity(burn) − m_fBoostBurnImmunity | HP × health_hit_part × hit_bone_scale; także kondycja |
| shock | O + H dla shock | 0 | GetHitImmunity(shock) − m_fBoostShockImmunity | HP × health_hit_part; także kondycja |
| chemical_burn | O + H dla chemical_burn | m_fBoostChemicalBurnProtection | GetHitImmunity(chemical_burn) − m_fBoostChemicalBurnImmunity | HP × health_hit_part; także kondycja |
| radiation | O + H dla radiation | m_fBoostRadiationProtection | GetHitImmunity(radiation) − m_fBoostRadiationImmunity | wzrost skażenia; bez bezpośredniego ubytku HP |
| telepatic | O + H dla telepatic | m_fBoostTelepaticProtection | GetHitImmunity(telepatic) − m_fBoostTelepaticImmunity | zdrowie psychiczne oraz HP × health_hit_part i kondycja |
| light_burn | O + H dla light_burn | 0 | GetHitImmunity(burn) − m_fBoostBurnImmunity | jak burn |

Odporności trudności w zainstalowanym
`ixray-stcop-wp-outfits/configs/creatures/actor.ltx` wynoszą dla pięciu typów
odpowiednio 0.30 / 0.70 / 0.85 / 1.00 (novice/stalker/veteran/master). UI nie używa
tych liczb jako stałych: `OnDifficultyChanged` ładuje dane do aktora, a nowy getter
czyta aktualny współczynnik z jego stanu. Testy używają różnych wartości dla typów.

## Szczególne przypadki i granice modelu

- `HitOutfitEffect` stosuje każdy faktycznie założony hełm, niezależnie od flagi
  kombinezonu `bIsHelmetAvaliable`. Nowe rozbicie odpowiada tej mechanice.
- Kopia ochrony `light_burn` może różnić się od `burn`, np. po ulepszeniu. Jeżeli
  skutkuje to innym efektywnym progiem, tooltip termiczny dopisuje
  `Próg dla light_burn`. Główne rozbicie O/H nadal dotyczy burn. Nazwa typu pozwala
  rozróżnić te dwa przypadki; light_burn nie występuje wyłącznie w polach ciągłych.
  Dotychczasowe Bieżące/Szczytowe nadal łączą oba typy w kanał termiczny.
- D nie jest clampowane w silniku. Pokazujemy również wartości >100%, 0 i ujemne.
  Przy D=0 faktyczne oddziaływanie znika także ponad progiem; nie zmieniamy jednak
  definicji progu warstw przed immunity. D<0 może odwrócić znak skutku.
- Ogólne `(P+B)/A` zakłada nieujemne płaskie warstwy. Helper odwraca je kolejno
  od końca: zaczyna od B, dodaje H, potem O; ujemny wynik na dowolnym etapie oznacza
  brak hitu możliwego do wyzerowania. UI pokazuje wtedy `Brak`, zamiast fałszywego
  progu 0. Nie zmienia to mechaniki ani współczynników. Nieprawidłowy/zerowy A także
  nie powoduje dzielenia przez zero. Zwykły wzór IX-Ray daje dodatnie A.
- Wartości opisują bazową ścieżkę ochrony. Callback OnBeforeHit może zmienić siłę
  lub typ przed pancerzem; GodMode i CanBeHarmed wpływają na rzeczywisty skutek.
  Odczyt oddziaływania pozostaje obserwacją faktycznych skutków.
- Regeneracja, health_hit_part, hit_bone_scale i kondycja nie wchodzą do D ani do
  efektywnego progu. D oznacza wyłącznie przejście H3 → H4.

## Skala i układ

Zgodnie z odpowiedzią użytkownika zachowano obecną skalę:

```
punkty(x, typ) = x / GetZoneMaxPower(typ) * 1000
```

Oba progi, O, H, B i siły Bieżące/Szczytowe używają tej samej skali. Dla maksimum
burn=0.2 surowe 0.20 oznacza 1000 pkt, a 0.2666667 — 1333.33 pkt. Procenty A i D
nie korzystają ze skali strefy. Wartości mają dotychczasową dokładność do dwóch
miejsc po przecinku, bez końcowych zer; bardzo małe niezerowe wartości mają `<0,01`.

Nowy opcjonalny `environment_hint_wnd` ma szerokość 280 UI i obszar tekstu 256 UI,
font `ui_font_panel_tt`. Dotyczy wyłącznie pięciu tooltipów środowiskowych. Inne
podpowiedzi zachowują własne okno. Kolumny liczb są wyrównane do 42 znaków widocznego
tekstu. Wysokość oblicza istniejące `UIHint::set_text` przez `AdjustHeightToText`,
a Draw mieści okno w ekranie. Starsze układy XML bez tego okna używają zwykłego
hint_wnd z automatycznym zawijaniem tekstu.

Kombinezon, hełm i efekt czasowy są widoczne tylko z dodatnim wkładem. Sekcja
artefaktów występuje tylko przy A != 1. Lista zawiera każdą założoną sztukę z
niezerowym `immunity[type] * condition`, bez indywidualnych procentów. Przeciwne
wkłady znoszące się do A=1 ukrywają sekcję. Przy małych współczynnikach float może
dać dokładnie A=1 — wtedy sekcja także jest ukryta.

Sekcja ODDZIAŁYWANIE zachowuje dotychczasowe warunki widoczności i źródła danych:
Bieżące to maksimum z 150 ms, Szczytowe z 750 ms; burn/light_burn mają wspólny kanał.
Ogień/chemia/prąd pokazują tempo skutków HP, psionika tempo skutków psychicznych.
Radiacja nadal pokazuje bieżące skażenie organizmu, z dotychczasowym kluczem
`ui_uip_unit_rad`, który w sprawdzonych polskich tekstach brzmi `rad`.
Starsza dokumentacja mówiąca tu o kBq nie odpowiada temu kodowi. Metod pomiaru,
okien czasowych, jednostek faktycznych skutków ani algorytmu DPS nie zmieniono.

**Panel po korekcie zgłoszenia +100 pkt:** liczba, pasek i trójkąt pięciu ochron
środowiskowych korzystają z tego samego efektywnego progu `(P+B)/A` co tooltip.
`GetEquipmentProtection` przestał dodawać surowe współczynniki artefaktów do pancerza.
Dla burn, referencji 0.2 i S=0.02 artefakt nie wnosi już błędnych 100 pkt: przy
pancerzu O=0.01 panel pozostaje na 50 pkt, a bez pancerza/boostera pozostaje na 0.
Brak osiągalnego progu jest pokazany jako 0 na panelu i „Brak” w tooltipie.
Pełny pasek nadal odpowiada 100 pkt; liczba nie jest ograniczana do 100.
Ochrony bojowe, mechanika obrażeń i karty przedmiotów pozostają bez zmian.

## Przykłady z produkcyjnego generatora tekstu

To kontrolowane dane testowe, nie stan zapisu gry i nie nowe parametry przedmiotów.
We wszystkich przykładach P=0.20, D=0.70 i referencja strefy=0.20. Przykładowy
artefakt testowy dobrano tak, aby rzeczywisty wzór dał A=0.75. W przykładzie chemii
B=0.05. Odczyty tempa w przykładach pochodzą z istniejącego obserwatora.

### Burn bez artefaktów

```text
Ochrona termiczna
. . . . . . . . . . . . . .

Efektywny próg:                   1000 pkt

Próg ochrony:                     1000 pkt
  Kombinezon:                      750 pkt
  Hełm:                            250 pkt

. . . . . . . . . . . . . .
Po przekroczeniu progu:
  Otrzymywane oddziaływanie:           70%
```

### Burn z artefaktem

```text
Ochrona termiczna
. . . . . . . . . . . . . .

Efektywny próg:                1333,33 pkt

Próg ochrony:                     1000 pkt
  Kombinezon:                      750 pkt
  Hełm:                            250 pkt

Wpływ artefaktów:                      25%
  Artefakt testowy

. . . . . . . . . . . . . .
Po przekroczeniu progu:
  Otrzymywane oddziaływanie:           70%

. . . . . . . . . . . . . .
ODDZIAŁYWANIE
Bieżące:                          2000 pkt
Szczytowe:                        3250 pkt
Obrażenia:                           7 %/s
```

### ChemicalBurn z boosterem protection

```text
Ochrona chemiczna
. . . . . . . . . . . . . .

Efektywny próg:                1666,67 pkt

Próg ochrony:                     1000 pkt
  Kombinezon:                      750 pkt
  Hełm:                            250 pkt
  Efekt czasowy:                   250 pkt

Wpływ artefaktów:                      25%
  Artefakt testowy

. . . . . . . . . . . . . .
Po przekroczeniu progu:
  Otrzymywane oddziaływanie:           70%

. . . . . . . . . . . . . .
ODDZIAŁYWANIE
Bieżące:                          2000 pkt
Szczytowe:                        2000 pkt
Obrażenia:                         3,5 %/s
```

## Weryfikacja

- Nowy `test_environmental_protection.py`: 3069 sprawdzeń produkcyjnych getterów,
  funkcji trafień i generatora tooltipów; dodatkowo tłumaczenia PL/EN/CZ/RU,
  geometria dedykowanego okna, automatyczna wysokość i powiązania.
- Przypadki A–F z wymagania: zaliczone. Próg sprawdzono trafieniami poniżej,
  dokładnie na granicy i powyżej; także wszystkie sześć typów, różne D, dodatnie
  i ujemne artefakty, sumowanie, stan, clamp ±0.99, ochrony czasowe i D<=0.
- Dotychczasowy `test_artefact_damage.py`: 43219 sprawdzeń i kolejność etapów.
- Dotychczasowy `test_environmental_damage.py`: 25 sprawdzeń oraz historia,
  wygasanie, zerowy efekt, GodMode i reset.
- Funkcje mechaniki, moduły pomiaru oddziaływania i delegaty panelu pozostają
  identyczne z wejściowym stanem zadania. EntityCondition otrzymał jedynie dwa
  gettery; jego `ConditionHit` i `HitOutfitEffect` są niezmienione.
- Szerszy stary `test_protection.py` miał wcześniej nieaktualne atrapy UI i nie
  kompilował się po zmianach tooltipów. Nowy test nie zależy od tych atrap.
  Nie przedstawiamy starego zestawu UI jako zaliczonego.
- Teksty przykładów zmierzono rzeczywistym polskim Consolas 11 z instalacji:
  maksymalna szerokość 253.97 przy polu 256, wysokość 12/21/22 wierszy.
  Jest to sprawdzenie tekstu i geometrii, nie zrzut działającej gry.

Uruchomienie:

```sh
ASAN_OPTIONS=detect_leaks=0 python3 tests/condition-ui/test_environmental_protection.py --mod-root /ścieżka/do/ixray-ui-params
```

Stan: zmiany źródłowe. Nie wykonano pełnej kompilacji Windows, instalacji nowego
silnika/dodatku ani próby w działającej grze. Nie zmieniano balansu artefaktów.

Korekta panelu: dodatkowe testy odtwarzają zgłoszone +100 pkt, sprawdzają brak
pancerza, rzeczywisty wpływ A=0.75, protection boostery oraz pięć produkcyjnych
delegatów XML. Mnożnik obrażeń aktora nadal nie wchodzi do progu.

Po korekcie panelu nowy zestaw testów przechodzi 3588 sprawdzeń, w tym pięć
delegatów liczb/pasków XML i zgodność panelu z efektywnym progiem tooltipa.
