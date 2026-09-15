# Poprawka pamięci animacji HUD

Samodzielny patch awarii odczytu definicji animacji w `player_hud::motion_length`.
Po zmianie modelu rąk odświeża animacje wszystkich zapamiętanych przedmiotów,
w tym schowanych. Sprawdza też slot i indeks przed odczytem definicji.

To eksport commita `5c10cedbe44ea62abb1d25796f88474e7a35ad75`, już obecnego
w zainstalowanym silniku. Nie wymaga panelu, zmian regeneracji ani jednostek.
Obejmuje pięć plików źródłowych i test w `tests/hud-motions/test_hud_motions.py`.
Nie ma osobnej gałęzi; przenośnym pakietem jest ten katalog.

Sprawdzono nałożenie na czysty upstream
`6c793faee008d83f86cec2429d39d7aa39b5bc66` i uruchomiono dołączony test
z ASan/UBSan. Test obejmuje zmianę modelu, wielokrotne ładowanie,
przedmioty widoczne i schowane oraz nieprawidłowe odwołania do animacji.
Pełny build Windows Release przeszedł dla zintegrowanego commita `5c10cedbe`.
Nie oznacza to potwierdzenia stabilności w rozgrywce ani przyszłych wersjach upstreamu.

## Użycie

Ustaw `PAKIET` na ścieżkę tego katalogu, a `IXRAY` na osobny, czysty checkout silnika.

```bash
python3 "$PAKIET/apply.py" --repo "$IXRAY"
python3 "$PAKIET/apply.py" --repo "$IXRAY" --apply
python3 "$IXRAY/tests/hud-motions/test_hud_motions.py"
```

Pierwsze polecenie tylko sprawdza zgodność. Drugie nakłada patch jako commit;
rozpoznaje też poprawkę już obecną. Potem skompiluj silnik dla swojej wersji
i sprawdź zmianę wyposażenia oraz dłuższe używanie ekwipunku.
Test wymaga Python 3 i g++ z obsługą ASan/UBSan. LeakSanitizer jest domyślnie
wyłączony, aby test działał także w środowisku z ptrace.

`patch.json` zawiera wersje i sumę SHA-256 eksportu. Przy aktualizacji IX-Ray
najpierw sprawdź patch na nowym checkoutcie; zgodność z nowszym kodem nie jest
zagwarantowana. Zintegrowanej gałęzi nie należy patchować ponownie.
