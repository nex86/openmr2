"""Create separate source .tar.gz and Windows/Linux ZIPs; never upload anything."""
from pathlib import Path
import argparse
import gzip
import hashlib
import io
import json
import subprocess
import sys
import tarfile
import zipfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_tar(path, root, files, prefix):
    with path.open('wb') as output:
        with gzip.GzipFile(filename='', mode='wb', fileobj=output, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode='w') as archive:
                for name in sorted(files):
                    data = (root/name).read_bytes()
                    entry = tarfile.TarInfo(prefix+'/'+name)
                    entry.size = len(data)
                    entry.mode = 0o755 if name.endswith('.sh') else 0o644
                    entry.mtime = 0
                    archive.addfile(entry, io.BytesIO(data))


def add_zip(archive, name, data, executable=False):
    info = zipfile.ZipInfo(name, (2026, 10, 19, 0, 0, 0))
    info.create_system = 3
    info.external_attr = (0o100755 if executable else 0o100644) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    archive.writestr(info, data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--windows', type=Path, required=True)
    parser.add_argument('--linux', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.source.resolve()
    subprocess.run([sys.executable, str(root/'tools/prepare_github.py'), '--check', str(root)], check=True)
    manifest = json.loads((root/'publication-manifest.json').read_text(encoding='utf-8'))
    version = manifest['version']
    files = {entry['path'] for entry in manifest['files']} | {'publication-manifest.json'}
    args.output.mkdir(parents=True, exist_ok=True)
    source = args.output/f'OpenMR2-{version}-source.tar.gz'
    outputs = [source]
    # Refuse accidental replacement of already reviewed release artifacts.
    targets = [source] + [args.output/f'OpenMR2-{version}-{platform}.zip'
                         for platform in ('Windows-x64', 'Linux-x86_64')]
    if any(path.exists() for path in targets):
        raise SystemExit('Output artifacts already exist; select a fresh output directory')
    windows = args.windows.read_bytes()
    linux = args.linux.read_bytes()
    pe = int.from_bytes(windows[0x3c:0x40], 'little')
    if windows[:2] != b'MZ' or windows[pe:pe+6] != b'PE\0\0\x64\x86':
        raise SystemExit('Expected a Windows x86-64 PE executable')
    if linux[:6] != b'\x7fELF\x02\x01' or linux[18:20] != b'\x3e\x00':
        raise SystemExit('Expected a Linux x86-64 ELF executable')
    source_tar(source, root, files, f'OpenMR2-{version}-source')
    source_hash = sha(source)
    documents = ['README.md', 'LICENSE', 'LICENSING.md', 'LICENSE_STATUS.md',
                 'PUBLICATION_AUDIT.md', 'THIRD_PARTY_NOTICES.txt', 'KEYBOARD.md',
                 'RECORDING.md', 'CONFIGURATION.md', 'custom_textures.txt',
                 'REVIVE_EFFECTS.md', 'RETRO_RENDERER.md', 'VIDEO_UPSCALING.md',
                 'CHANGELOG.md', 'RELEASE_NOTES.md', 'DISTRIBUTION.md', 'BUILDING.md']
    documents += sorted(name for name in files if name.startswith('docs/'))
    # Preserve complete notices and separately rebuildable shader sources.
    documents += sorted(name for name in files if name.startswith('src/third_party/'))
    documents += ['assets/fonts/OFL.txt', 'assets/fonts/README.txt']
    details = {
        'version': version, 'source_archive': source.name, 'source_sha256': source_hash,
        'source_repository': 'https://github.com/nex86/openmr2',
        'unresolved_rights': manifest['unresolved'],
        'ffmpeg_bundled': False, 'original_game_data_bundled': False,
        'binaries': {},
    }
    for platform, path, name in (
        ('Windows-x64', args.windows, 'OpenMR2.exe'),
        ('Linux-x86_64', args.linux, 'openmr2_linux'),
    ):
        prefix = f'OpenMR2-{version}-{platform}'
        destination = args.output/(prefix+'.zip')
        source_info = (
            f'OpenMR2 {version}\nSource repository: https://github.com/nex86/openmr2\n'
            f'Complete corresponding project source: {source.name}\nSHA256: {source_hash}\n'
            'Publish the source tarball alongside these ZIPs in the same GitHub release.\n'
            'Dependency sources and licenses are included in that tarball.\n'
            'No game files or FFmpeg encoder are bundled in this ZIP.\n'
            'See LICENSING.md and PUBLICATION_AUDIT.md for unresolved rights findings.\n'
        )
        with zipfile.ZipFile(destination, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            add_zip(archive, prefix+'/'+name, path.read_bytes(), executable=True)
            add_zip(archive, prefix+'/SOURCE.txt', source_info.encode('utf-8'))
            for doc in sorted(set(documents)):
                add_zip(archive, prefix+'/'+doc, (root/doc).read_bytes())
        with zipfile.ZipFile(destination) as archive:
            assert archive.testzip() is None
            assert archive.read(prefix+'/'+name) == path.read_bytes()
            assert not any(member.lower().endswith(('.gog', '.iso', '.cfg', '.sav'))
                           or member.lower().endswith('/ffmpeg.exe') for member in archive.namelist())
        outputs.append(destination)
        details['binaries'][platform] = {'file': name, 'sha256': sha(path)}
    with tarfile.open(source, 'r:gz') as archive:
        members = archive.getmembers()
        assert len(members) == len(files)
        prefix = f'OpenMR2-{version}-source/'
        for member in members:
            name = member.name.removeprefix(prefix)
            assert name in files and member.isfile()
            assert archive.extractfile(member).read() == (root/name).read_bytes()
    sums = ''.join(sha(path)+'  '+path.name+'\n' for path in outputs)
    (args.output/'SHA256SUMS.txt').write_text(sums, encoding='utf-8')
    details['archives'] = {path.name: {'sha256': sha(path), 'bytes': path.stat().st_size}
                           for path in outputs}
    (args.output/'release-inventory.json').write_text(json.dumps(details, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(details['archives'], indent=2))


if __name__ == '__main__':
    main()
