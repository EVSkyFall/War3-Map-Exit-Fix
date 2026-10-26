"""Run fixed-layout offline native fixtures using privately supplied DLLs."""
import argparse
import hashlib
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--dz', required=True, type=Path)
    args = parser.parse_args()
    game, dz = args.game.resolve(), args.dz.resolve()
    # These fingerprints restrict fixed-address diagnostic execution only.
    fixtures = [(game, '05285a28e5c55a0be0c2948ee7e3b41426a1b6d74359b7d3ea57ea322f690420'),
                (dz, 'cb0f0c9b145d7a1c9e366733fc138af260f944fba2c95adf6664840e151457c0')]
    for path, expected in fixtures:
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise SystemExit(f'{path.name}: this fixed-address diagnostic requires the fixture documented in docs/building.md.')
    map_dir, post_dir = ROOT / 'build/map-exit', ROOT / 'build/post-game'
    map_mix, post_mix = map_dir / 'War3MapExitFix.mix', post_dir / 'War3PostGameFix.mix'
    commands = [
        [map_dir / 'map-exit-test.exe', game, map_mix],
        [post_dir / 'post-game-test.exe', game, dz, post_mix, map_mix],
        [post_dir / 'post-game-test.exe', game, dz, post_mix, map_mix, 'active-retry'],
        [post_dir / 'menu-cursor-test.exe', game, dz, post_mix, map_mix],
    ]
    for command in commands:
        subprocess.run([str(value) for value in command], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
