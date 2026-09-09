# Naliczanie czasu efektów sprzętu

Samodzielny patch silnika IX-Ray: artefakty i kombinezon uwzględniają cały
upływający czas, także z aktualizacji, która uruchamia naliczenie efektów.
Licznik należy do obiektu gracza i zeruje się przy reinicjalizacji/wczytaniu.

Patch obejmuje tylko trzy pliki: `Actor.cpp`, `Actor.h` i `Actor_Network.cpp`
w `src/xrGame`. Nie wymaga patcha kodowania znaków, zmienionego panelu,
tooltipów, kBq ani konfiguracji dodatku. Współczynniki z konfiguracji pozostają
bez zmian, ale pełniejsze naliczanie czasu wzmacnia dotychczasowe efekty
sprzętu.

## Pliki i gałąź

- `0001-fix-equipment-condition-time.patch` — jeden samodzielny commit.
- `apply.py` — sprawdzenie zgodności i opcjonalne nałożenie jako commit.
- `verify_clock.py` — test zachowania czasu na rzeczywistej funkcji z podanego
  checkoutu, z małymi zastępnikami klas silnika; wymaga Python 3 i `g++`.
- `patch.json` — baza, commit, suma kontrolna i lista plików.
- Gałąź źródłowa: `codex/equipment-condition-time`, jeden commit ponad czystym
  upstreamem. Gałąź z pracami nad panelem pozostaje osobno.

Aktualny eksport przygotowano na bazie `6c793faee008d83f86cec2429d39d7aa39b5bc66`
(lokalnie dostępny `upstream/default`, commit z 6 września 2026).
Sprawdzenie tego punktu historii nie oznacza sprawdzenia przyszłych wydań.

## Nałożenie na nową wersję IX-Ray

Pobierz źródła nowej wersji i użyj ich osobnego, czystego checkoutu.
W poniższych przykładach `PAKIET` oznacza pełną ścieżkę do tego katalogu,
a `NOWY_IXRAY` — pełną ścieżkę do źródeł silnika.

```bash
python3 PAKIET/apply.py --repo NOWY_IXRAY
python3 PAKIET/apply.py --repo NOWY_IXRAY --apply
python3 PAKIET/verify_clock.py --repo NOWY_IXRAY
```

Pierwsze polecenie wyłącznie sprawdza. Drugie nakłada patch jako jeden commit.
Skrypt rozpoznaje już nałożoną poprawkę, nie zmienia gałęzi, nie pobiera niczego
z sieci i niczego nie wysyła. Po teście nadal potrzebna jest kompilacja silnika
oraz sprawdzenie działania w grze.

Można też nałożyć patch bez skryptu:

```bash
git -C NOWY_IXRAY apply --check PAKIET/0001-fix-equipment-condition-time.patch
git -C NOWY_IXRAY am --keep-cr --3way PAKIET/0001-fix-equipment-condition-time.patch
```

`--keep-cr` zachowuje zakończenia linii CRLF. W razie konfliktu przejrzyj
`git status`; wycofanie rozpoczętego nakładania to `git am --abort`.

## Aktualizacja patcha

Gałąź `codex/equipment-condition-time` utrzymuj jako samą poprawkę na bazie
wybranej wersji upstreamu. Można przenieść jej pojedynczy commit na nową bazę
przez rebase/cherry-pick w osobnym checkoutcie, rozwiązać ewentualne konflikty,
uruchomić test i dopiero wtedy wyeksportować commit ponownie:

```bash
git format-patch -1 HEAD --stdout --full-index --no-signature > PAKIET/0001-fix-equipment-condition-time.patch
```

Eksportuj w ten sposób tylko właściwy commit poprawki czasu. Po zmianie
eksportu zaktualizuj metadane `patch.json` i ponownie sprawdź nałożenie
na czystej bazie. Zmiana numerów linii sama w sobie zwykle nie przeszkadza
Gitowi; zmiana otaczającego kodu może wymagać ręcznego przeniesienia poprawki.

Jeśli upstream sam naprawi naliczanie, porównaj zachowanie i usuń zbędny patch
z własnego zestawu. Nie wymuszaj drugiego nakładania równoważnej poprawki.

Katalog jest celowo pod `patches/equipment-condition-time/`. Wcześniejszy
skrypt utrzymania patcha fontów czyści `patches/*.patch`; osobny podkatalog
chroni tę poprawkę przed jego czyszczeniem.
