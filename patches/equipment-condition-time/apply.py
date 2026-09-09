#!/usr/bin/env python3
"""Sprawdź patch; --apply nakłada go jako commit na czyste repozytorium."""
from pathlib import Path
import argparse
import subprocess
import sys

HERE = Path(__file__).resolve().parent
PATCH = HERE / '0001-fix-equipment-condition-time.patch'


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
    # must not be applied a second time, even if unrelated files have changed.
    if git('apply', '--reverse', '--check', str(PATCH)).returncode == 0:
        print('Ta poprawka jest już obecna. Nie ma nic do nakładania.')
        return 0
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
