# ixray-1.6-stcop — silnik gry (fork IX-Ray)

Fork `ixray-team/ixray-1.6-stcop`. Tu żyją zmiany w C++ potrzebne dodatkom z
`/home/tmz/Projects/ixray-addons/`. Dane gry (XML, LTX, tekstury) **nie** są tu
utrzymywane — mieszkają w repozytoriach dodatków.

## Zasady wspólne projektu

Gałęzie, wdrażanie, mechanika dodatków i konwencje są opisane w
`/home/tmz/Projects/ixray-docs/README.md` — **przeczytaj to przed pierwszą zmianą.**

@/home/tmz/Projects/ixray-docs/README.md

## Co jest specyficzne dla tego repozytorium

- **Praca idzie na `feature/<temat>`, a po sprawdzeniu w grze scala się do `build/tmz`**
  jawnym `merge --no-ff`. Gałęzi `default` nie dotykamy — jest lustrem upstreamu.
- **Zmiana w C++ nie działa w grze, dopóki nie przejdzie przez CI i instalację binarek.**
  To najczęstsza pomyłka w tym projekcie; zanim uznasz poprawkę za niedziałającą,
  sprawdź sha binarki (`strings <gra>/bin/xrEngine.exe | grep -m1 feature/`).
- **Komentarze w kodzie po angielsku**, opisy commitów po polsku. Komentarze jadą
  z patchem do cudzego drzewa, opisy commitów nie.
- Dostosowuj się do formatowania otoczenia (tabulatory, `IC`, `R_ASSERT`) i nie
  refaktoryzuj przy okazji — pakiety poprawek muszą dać się nałożyć na inną wersję.
- **Gałąź źródłowa `fix/*` / `feature/*` niesie kod poprawki i testy, a `patches/<nazwa>/`
  na `build/tmz` to przenośne artefakty pakietów** generowane z tej gałęzi. `.gitignore`
  wyłącza sam katalog `patches/` z upstreamowej reguły `patch*/`, więc `git status`
  i zwykłe `git add` działają. Pozostałe reguły obowiązują dalej, także wewnątrz
  `patches/` (`*.log`, `patch_tmp/`).
- Testy w `tests/` uruchamia się wprost (`python3 tests/<rodzina>/<test>.py`); **żadne
  CI ich nie odpala**. Wymagają `g++` z ASan/UBSan.

## Dokumentacja

- `patches/<nazwa>/README.md` — opis konkretnej poprawki: problem, rozwiązanie, co
  sprawdzono, jaki jest status próby w grze.
- `doc/` i `docs/` należą do **upstreamu** (styl kodu, model gałęzi, strona VitePress).
  Nie dopisuj tam własnych dokumentów — kolidują przy synchronizacji.

Rzeczy wspólne całego projektu trzymamy w `ixray-docs`. Nie kopiuj ich tutaj — linkuj.
