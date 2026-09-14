#!/usr/bin/env python3
"""Sprawdź patch; --apply nakłada go jako commit na czyste repozytorium."""
from pathlib import Path
import argparse
import subprocess
import sys

HERE = Path(__file__).resolve().parent
PATCH = HERE / '0001-fix-inventory-cell-grid.patch'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, default=Path.cwd(), help='Repozytorium źródeł IX-Ray (domyślnie bieżący katalog).')
    parser.add_argument('--apply', action='store_true', help='Nałóż patch; bez tej opcji tylko sprawdź zgodność.')
    args = parser.parse_args()

    def git(*arguments):
        return subprocess.run(['git', '-C', str(args.repo), '-c', 'core.whitespace=cr-at-eol', *arguments],
                              capture_output=True, text=True, errors='replace')

    root = git('rev-parse', '--show-toplevel')
    if root.returncode:
        print('To nie jest repozytorium Git: ' + str(args.repo), file=sys.stderr)
        return 2
    args.repo = Path(root.stdout.strip())

    # These commands only inspect the working tree. A previously installed patch
    # must not be applied a second time, even if unrelated files have changed. This
    # runs before the dependency check: with the patch already in, the dependencies
    # are in by construction, and they can no longer be detected the way below.
    if git('apply', '--reverse', '--check', str(PATCH)).returncode == 0:
        print('Ta poprawka jest już obecna. Nie ma nic do nakładania.')
        return 0

    # The new geometry is wired into both earlier packages: it changes the cell size
    # quantize_grab_offset rounds with and the one the drop preview draws with. On its
    # own it would apply and leave those two reading a size nothing else uses.
    #
    # Detection is by marker, not by reverse-applying the dependency patches: this patch
    # rewrites lines that inventory-drop-cell introduced, so once it is in, a reverse
    # check of that patch no longer succeeds. The markers below survive it.
    dependencies = [
        ('inventory-drop-cell', 'src/xrGame/ui/UICellItem.h', 'm_drop_offset'),
        ('inventory-drop-preview', 'src/xrGame/ui/UIDragDropListEx.h', 'DrawDropPreview'),
    ]
    for name, marker_file, marker in dependencies:
        if not (HERE.parent / name).is_dir():
            print('Brakuje pakietu zaleznosci obok tego katalogu: ' + str(HERE.parent / name), file=sys.stderr)
            return 2
        path = args.repo / marker_file
        if not path.is_file() or marker not in path.read_text(errors='replace'):
            print('Najpierw naloz pakiet ' + name + '; ten go rozszerza.', file=sys.stderr)
            return 2
    check = git('apply', '--check', str(PATCH))
    if check.returncode:
        print('Patch wymaga przeglądu na tej wersji IX-Ray. Repozytorium pozostało bez zmian.', file=sys.stderr)
        print(check.stderr.strip(), file=sys.stderr)
        return 1
    if not args.apply:
        print('Patch pasuje do tej wersji. Aby go nałożyć, dodaj --apply.')
        return 0

    status = git('status', '--porcelain')
    if status.returncode or status.stdout.strip():
        print('Nakładanie wymaga czystego katalogu roboczego. Użyj osobnego, czystego checkoutu; skrypt nie chowa ani nie usuwa zmian.', file=sys.stderr)
        return 2
    for name in ['rebase-apply', 'rebase-merge', 'MERGE_HEAD', 'CHERRY_PICK_HEAD', 'REVERT_HEAD']:
        location = git('rev-parse', '--git-path', name)
        if location.returncode:
            return 2
        path = Path(location.stdout.strip())
        if not path.is_absolute():
            path = args.repo / path
        if path.exists():
            print('Najpierw zakończ bieżącą operację Git: ' + name, file=sys.stderr)
            return 2

    # --keep-cr preserves the engine's CRLF lines inside the mail-format patch.
    result = git('am', '--keep-cr', '--3way', str(PATCH))
    if result.returncode:
        print(result.stdout + result.stderr, file=sys.stderr)
        print('Jeżeli rozpoczęło się nakładanie i wystąpił konflikt, sprawdź git status. Powrót: git am --abort.', file=sys.stderr)
        return 1
    print('Nałożono poprawkę jako commit ' + git('rev-parse', '--short', 'HEAD').stdout.strip() + '.')
    print('Pozostają kompilacja silnika i próba w grze.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
