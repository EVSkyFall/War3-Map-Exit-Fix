"""Build Windows x86 map-exit plugins using LLVM-MinGW."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path)
    parser.add_argument('--component', choices=['all', 'map-exit', 'post-game'], default='all')
    parser.add_argument('--tests', action='store_true', help='Compile optional local native replay executables')
    args = parser.parse_args()
    components = ['map-exit', 'post-game'] if args.component == 'all' else [args.component]
    for component in components:
        build(component, args)


def build(component, args):
    post_game = component == 'post-game'
    unit = component.replace('-', '_')
    out = ROOT / 'build' / component
    out.mkdir(parents=True, exist_ok=True)
    def compiler(name):
        if os.name == 'nt':
            name += '.exe'
        return str(args.toolchain.resolve() / 'bin' / name) if args.toolchain else shutil.which(name)
    cc, cxx = compiler('i686-w64-mingw32-clang'), compiler('i686-w64-mingw32-clang++')
    flags = ['-O2', '-Wall', '-Wextra', '-Wno-unused-parameter', '-fms-extensions', '-fasm-blocks',
             '-ffile-prefix-map=' + str(ROOT) + '=.', '-Isrc', '-Ivendor/minhook/include',
             '-D_WIN32_WINNT=0x0601', '-D_CRT_SECURE_NO_WARNINGS']
    def run(command):
        subprocess.run(command, cwd=ROOT, check=True)
    objects = []
    for source in ['src/' + unit + '.cpp'] + (['src/menu_cursor.cpp'] if post_game else []) + ['vendor/minhook/src/' + name for name in
                                        ['buffer.c', 'hook.c', 'trampoline.c', 'hde/hde32.c']]:
        obj = out / (Path(source).stem + '.o')
        run([cxx if source.endswith('.cpp') else cc, *flags, '-c', source, '-o', str(obj)])
        objects.append(str(obj))
    guard = out / 'guard.o'
    run([cc, '--target=i686-pc-windows-msvc', '-O2', '-c', 'src/guard.c', '-o', str(guard)])
    objects.append(str(guard))
    target = out / ('War3PostGameFix.mix' if post_game else 'War3MapExitFix.mix')
    run([cxx, '-shared', '-static', '-Wl,--no-insert-timestamp', '-Wl,--dynamicbase', '-Wl,--nxcompat',
         '-o', str(target), *objects])
    if args.tests:
        run([cxx, *flags, 'tests/' + unit + '.cpp', *objects, '-static', '-o', str(out / (component + '-test.exe'))])
    if args.tests and post_game:
        run([cxx, *flags, 'tests/menu_cursor.cpp', *objects, '-static', '-o', str(out / 'menu-cursor-test.exe')])
    manifest = {'mix_sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
                'compiler': subprocess.check_output([cxx, '--version'], text=True).splitlines()[0],
                'bytes': target.stat().st_size}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(target)


if __name__ == '__main__':
    main()
