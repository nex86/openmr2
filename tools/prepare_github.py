"""Export canonical development sources to an existing local GitHub clone.

Never uploads, stages or commits. A complete source snapshot includes the
unresolved components identified in the documentation's publication audit. Technical export
validation is not a determination of redistribution rights.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from publication_docs import MANUAL, consolidate_documents

ROOT = Path(__file__).resolve().parents[1]
TREES = {
    'src': {'.cpp', '.h', '.inc', '.txt', '.md', '.json', '.glsl', '.cg', '.png', '.glslp'},
    'assets': {'.png', '.ttf', '.txt', '.md', '.json'},
    'tests': {'.cpp', '.inc', '.py', '.ps1', '.sh'},
    'tools': {'.py'},
    'winlib': {'.def'},
    '.github': {'.yml', '.yaml', '.md'},
    'docs': {'.md', '.png', '.json'},
}
DOCS = (
    'BUILDING.md CHANGELOG.md CONFIGURATION.md CONTRIBUTING.md ENGINE_AUDIO.md '
    'HUD_RESEARCH.md LICENSE_STATUS.md LICENSING.md LOCALIZATION_REVIEW.md '
    'PUBLICATION_AUDIT.md RECORDING.md RELEASE_NOTES.md RETRO_RENDERER.md PHYSICS.md '
    'REVIVE_EFFECTS.md THIRD_PARTY_NOTICES.txt TODO.md VIDEO_UPSCALING.md '
    'custom_textures.txt localization.tsv build_windows.ps1 build_win.sh build_linux.sh '
    'KEYBOARD.md DISTRIBUTION.md'
).split()
EXCLUDED_TOOLS = {
    'package_candidates.py', 'package_candidate_0713.py',
    'record_validation_0712.py', 'record_validation_0713.py',
}
FORBIDDEN_PARTS = {'revived_config', 'texture_dumps', 'texture_replacements',
                   'recordings', 'screenshots', '__pycache__', 'dosbox'}
FORBIDDEN_SUFFIXES = {'.exe', '.dll', '.lib', '.obj', '.o', '.so', '.a', '.appimage',
                      '.gog', '.iso', '.bin', '.cue', '.ubb', '.hlz', '.spr',
                      '.wav', '.mp3', '.mp4', '.jpg', '.jpeg', '.cfg', '.sav',
                      '.zip', '.7z', '.pyc', '.log', '.pem', '.pfx', '.key'}
TEXT_SUFFIXES = {'.cpp', '.h', '.inc', '.txt', '.md', '.json', '.tsv', '.def',
                 '.py', '.ps1', '.sh', '.yml', '.yaml', '.glsl', '.cg', '.glslp'}
CHECKS = {
    'private home path': re.compile(rb'(?:[A-Za-z]:[/\\]+Users[/\\]+[A-Za-z0-9_-]|/mnt/[a-z]/[Uu]sers/|/home/[A-Za-z0-9_-]+/)'),
    'GitHub credential': re.compile(rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})'),
    'private key': re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'),
    'AWS access key': re.compile(rb'AKIA[0-9A-Z]{16}'),
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def classification(name):
    if name == 'src/ubbdec.inc':
        return 'P01: original executable translation; rights unresolved'
    if name.startswith('assets/revived_ui/') or name == 'src/remastered_ui_data.h':
        return 'P02: AI-edited original artwork; rights unresolved'
    if name.startswith('src/third_party/') or name in (
        'src/output_filter_data.h', 'src/video_filter_data.h', 'src/video_fsr_data.h'):
        return 'third-party component; upstream terms retained'
    if name.startswith('assets/fonts/') or name == 'src/revived_font_data.h':
        return 'SIL OFL font and derived atlas'
    if name.startswith('assets/effects/') or name == 'src/fire_atlas_data.h':
        return 'text-generated project fire asset'
    if name == 'LICENSE':
        return 'GPL version 3 license text; scope in DOCUMENTATION.md#licensing'
    return 'project development material; scope in DOCUMENTATION.md#licensing'


def selected():
    result = {}
    for folder, extensions in TREES.items():
        for path in sorted((ROOT / folder).rglob('*')):
            if not path.is_file() or '__pycache__' in path.parts:
                continue
            if path.suffix.lower() not in extensions:
                continue
            if folder == 'tools' and path.name in EXCLUDED_TOOLS:
                continue
            result[path.relative_to(ROOT).as_posix()] = path
    for name in DOCS:
        path = ROOT / name
        if path.exists() or not name.endswith('.md'):
            result[name] = path
    if (ROOT / MANUAL).exists():
        result[MANUAL] = ROOT / MANUAL
    result['README.md'] = ROOT / ('README_GITHUB.md' if (ROOT / 'README_GITHUB.md').exists() else 'README.md')
    return result


def file_findings(path, name):
    findings = []
    parts = {p.lower() for p in Path(name).parts}
    if parts & FORBIDDEN_PARTS or path.suffix.lower() in FORBIDDEN_SUFFIXES:
        findings.append((name, 'prohibited payload'))
    if path.name.startswith('.env') or path.is_symlink():
        findings.append((name, 'environment file or symlink'))
    if path.stat().st_size > 90 * 1024 * 1024:
        findings.append((name, 'file exceeds source export size limit'))
    if path.suffix.lower() in TEXT_SUFFIXES or path.name.startswith('.') or path.name == 'LICENSE':
        data = path.read_bytes()
        for label, pattern in CHECKS.items():
            if pattern.search(data):
                # Report only a category and filename; never echo credential contents.
                findings.append((name, label))
    return findings


def check(folder):
    manifest = json.loads((folder / 'publication-manifest.json').read_text(encoding='utf-8'))
    findings = []
    expected = set()
    for item in manifest['files']:
        name = item['path']
        if Path(name).is_absolute() or '..' in Path(name).parts:
            raise SystemExit('Unsafe path in manifest')
        expected.add(name)
        path = folder / name
        if not path.is_file():
            findings.append((name, 'missing file'))
            continue
        findings.extend(file_findings(path, name))
        if digest(path) != item['sha256']:
            findings.append((name, 'SHA-256 mismatch; regenerate the export manifest'))
    expected.add('publication-manifest.json')
    if manifest.get('documentation', {}).get('format') == 'single-manual':
        markdown = {name for name in expected if name.lower().endswith('.md')}
        if markdown != {'README.md', MANUAL}:
            findings.append((MANUAL, 'publication must contain only README.md and DOCUMENTATION.md'))
    actual = set()
    for path in folder.rglob('*'):
        relative = path.relative_to(folder)
        if '.git' in relative.parts or relative.parts[0] == 'build':
            continue
        if path.is_file():
            name = relative.as_posix()
            # Builds generate import libraries from the tracked .def files.
            if relative.parts[0] == 'winlib' and path.suffix == '.lib':
                continue
            actual.add(name)
            if name not in expected:
                findings.append((name, 'unexpected file outside manifest'))
    for name in sorted(expected - actual):
        findings.append((name, 'absent from export'))
    for name, reason in findings:
        print(f'{reason}: {name}')
    if findings:
        raise SystemExit(f'Export checks failed: {len(findings)} findings')
    print(f'Export checks passed: {len(manifest["files"])} files; no prohibited payload or known credential pattern.')
    print('Rights status remains separate: P01 decoder / P02 derivative art unresolved.')


def export(destination):
    destination = destination.resolve()
    if destination == ROOT or ROOT.is_relative_to(destination):
        raise SystemExit('Destination must not overwrite the working source tree or its parents')
    if not (destination / '.git').is_dir():
        raise SystemExit('Clone the target repository first; destination must have its own .git directory')
    status = subprocess.check_output(['git', '-C', str(destination), 'status', '--porcelain'], text=True)
    if status.strip():
        raise SystemExit('Destination has changes; preserve/review them before regenerating the export')
    paths = selected()
    findings = [finding for name, path in paths.items() for finding in file_findings(path, name)]
    if findings:
        for name, reason in findings:
            print(f'{reason}: {name}')
        raise SystemExit('Source inspection failed; no files exported')
    markdown = {name: path.read_text(encoding='utf-8') for name, path in paths.items() if name.endswith('.md')}
    documents = consolidate_documents(markdown)
    previous_manifest = destination / 'publication-manifest.json'
    old_files = json.loads(previous_manifest.read_text(encoding='utf-8'))['files'] if previous_manifest.exists() else []
    # Retire only Markdown files from the previous reviewed publication inventory.
    for item in old_files:
        name = item['path']
        if name.endswith('.md') and name not in documents:
            target = (destination / name).resolve()
            if not target.is_relative_to(destination):
                raise SystemExit('Unsafe obsolete documentation path')
            if target.is_file():
                target.unlink()
    paths = {name: path for name, path in paths.items() if not name.endswith('.md')}
    for name, source in paths.items():
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    for name, text in documents.items():
        target = destination / name
        target.write_text(text, encoding='utf-8', newline='\n')
        paths[name] = target
    # Preserve the existing repository's GPL text, never replace it with guessed ownership.
    license_path = destination / 'LICENSE'
    if not license_path.is_file():
        raise SystemExit('Expected the existing repository GPL-3.0 LICENSE')
    paths['LICENSE'] = license_path
    ignore = destination / '.gitignore'
    required = ('/revived_config/ /texture_dumps/ /texture_replacements/ '
                '*.gog *.iso *.bin *.cue *.cfg *.sav *.AppImage *.zip *.7z '
                '__pycache__/ *.pyc .env .env.*').split()
    existing = ignore.read_text(encoding='utf-8') if ignore.exists() else ''
    missing = [line for line in required if line not in existing.splitlines()]
    if missing:
        with ignore.open('a', encoding='utf-8', newline='\n') as stream:
            stream.write('\n# Original game files and personal runtime output\n'+'\n'.join(missing)+'\n')
    paths['.gitignore'] = ignore
    attributes = destination / '.gitattributes'
    attributes.write_text('* text=auto eol=lf\n*.sh text eol=lf\n*.py text eol=lf\n'
                          '*.cpp text eol=lf\n*.h text eol=lf\n*.inc text eol=lf\n'
                          '*.yml text eol=lf\n*.png binary\n*.ttf binary\n', encoding='utf-8', newline='\n')
    paths['.gitattributes'] = attributes
    # Hash the exact LF representation that Git will store and both CI systems
    # will check out. Otherwise autocrlf would invalidate the export inventory.
    for name in paths:
        path = destination / name
        if path.suffix.lower() in TEXT_SUFFIXES or path.name.startswith('.') or name == 'LICENSE':
            data = path.read_bytes()
            normalized = data.replace(b'\r\n', b'\n')
            if normalized != data:
                path.write_bytes(normalized)
    manifest = {
        'date': '2026-10-19',
        'target': 'https://github.com/nex86/openmr2',
        'version': '0.7.15-dev',
        'classification': 'complete local development source snapshot, not legal clearance',
        'original_game_data_included': False,
        'bundled_executables_included': False,
        'unresolved': ['P01 decoder translation', 'P02 AI-edited original artwork'],
        'documentation': {'format': 'single-manual', 'manual': MANUAL,
                          'source_documents': sorted(set(markdown)-{'README.md', MANUAL})},
        'files': [{'path': name, 'bytes': (destination/name).stat().st_size,
                   'sha256': digest(destination/name), 'classification': classification(name)}
                  for name in sorted(paths)],
    }
    (destination/'publication-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8', newline='\n')
    check(destination)


def refresh(folder):
    """Rebuild the inventory after intentional edits; validate every exported file."""
    manifest_path = folder / 'publication-manifest.json'
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    names = subprocess.check_output(
        ['git', '-C', str(folder), 'ls-files', '--cached', '--others', '--exclude-standard', '-z']
    ).decode('utf-8').split('\0')
    files = []
    for name in sorted(set(names) - {'', 'publication-manifest.json'}):
        path = folder / name
        if not path.is_file():
            continue
        if file_findings(path, name):
            raise SystemExit('Cannot inventory prohibited/private material: '+name)
        files.append({'path': name, 'bytes': path.stat().st_size,
                      'sha256': digest(path), 'classification': classification(name)})
    manifest['files'] = files
    manifest_path.write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8', newline='\n')
    check(folder)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--export', type=Path, metavar='LOCAL_CLONE')
    group.add_argument('--check', type=Path, metavar='SOURCE_EXPORT')
    group.add_argument('--refresh', type=Path, metavar='SOURCE_EXPORT')
    args = parser.parse_args()
    if args.export:
        export(args.export)
    elif args.refresh:
        refresh(args.refresh.resolve())
    else:
        check(args.check.resolve())
