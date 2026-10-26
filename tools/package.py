"""Package the two built plugins and their documentation into a release ZIP."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', required=True)
    args = parser.parse_args()
    files = {}
    for folder, name in [('map-exit', 'War3MapExitFix.mix'), ('post-game', 'War3PostGameFix.mix')]:
        output = ROOT / 'build' / folder
        data = (output / name).read_bytes()
        manifest = json.loads((output / 'manifest.json').read_text(encoding='utf-8'))
        if hashlib.sha256(data).hexdigest() != manifest['mix_sha256']:
            raise SystemExit(f'{name} differs from its build manifest; rebuild before packaging.')
        files[name] = data
    for name in ['README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'CHANGELOG.md',
                 'docs/building.md', 'docs/validation.md', 'vendor/minhook/LICENSE.txt']:
        files[name] = (ROOT / name).read_bytes()
    checksums = ''.join(f'{hashlib.sha256(data).hexdigest()}  {name}\n' for name, data in sorted(files.items()))
    files['SHA256SUMS.txt'] = checksums.encode('utf-8')
    dist = ROOT / 'dist'
    dist.mkdir(exist_ok=True)
    archive = dist / f'War3-Map-Exit-Fix-v{args.version.removeprefix("v")}.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as out:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            out.writestr(info, data)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix('.sha256').write_text(f'{digest}  {archive.name}\n', encoding='utf-8')
    print(archive)
    print(digest)


if __name__ == '__main__':
    main()
