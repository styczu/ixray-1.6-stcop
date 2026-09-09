"""Compile the production formatter with legacy snprintf macros in scope."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
cxx = os.environ.get('CXX', 'g++')
checks = r'''
int main()
{
    char number[32];
    ConditionUi::FormatNumber(number, 0.8, ',', true);
    assert(std::strcmp(number, "+0,8") == 0);
    ConditionUi::FormatNumber(number, 0.002, ',', true);
    assert(std::strcmp(number, "+<0,01") == 0);
    ConditionUi::FormatNumber(number, -0.002, ',', true);
    assert(std::strcmp(number, "-<0,01") == 0);
    ConditionUi::FormatNumber(number, 0.0, ',', true);
    assert(std::strcmp(number, "0") == 0);
    ConditionUi::FormatNumber(number, 10.2, '.', true);
    assert(std::strcmp(number, "+10.2") == 0);
    char small[8];
    ConditionUi::FormatNumber(small, 123456789.0, '.', true);
    assert(small[7] == '\0');
    MACRO_CHECK
}
'''
cases = {
    'no_macro': ('', '#ifdef snprintf\n#error Formatter leaked a macro\n#endif\n', ''),
    'gamespy_object_macro': (
        '#define snprintf _snprintf\n',
        '#ifndef snprintf\n#error Caller macro was not restored\n#endif\n',
        'assert(std::strcmp(STRINGIFY(snprintf), "_snprintf") == 0);'),
    'function_macro': (
        '#define snprintf(...) legacy_format(__VA_ARGS__)\n',
        '#ifndef snprintf\n#error Caller macro was not restored\n#endif\n',
        'assert(std::strcmp(STRINGIFY(snprintf(42)), "legacy_format(42)") == 0);'),
}
with tempfile.TemporaryDirectory(prefix='ixray-format-macros-') as directory:
    for preloaded in (False, True):
        for name, (macro, post, check) in cases.items():
            source = Path(directory) / f'{name}_{preloaded}.cpp'
            source.write_text(
                ('#include <cstdio>\n' if preloaded else '') + macro +
                '#include "ConditionUiValues.h"\n' + post +
                '#include <cassert>\n#define STRINGIFY_INNER(x) #x\n'
                '#define STRINGIFY(x) STRINGIFY_INNER(x)\n' +
                checks.replace('MACRO_CHECK', check))
            binary = source.with_suffix('')
            subprocess.run([cxx, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-I', str(root / 'src/xrGame'), str(source), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
print('PASS: 6 formatter compilation variants; formatting, truncation and caller macro restoration')
