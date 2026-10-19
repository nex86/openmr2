"""Exercise publication checks against corrupt/unsafe temporary exports."""
from pathlib import Path
import contextlib
import hashlib
import importlib.util
import io
import json
import tempfile
import unittest

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('prepare_github', root/'tools/prepare_github.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class ExportChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.folder = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def manifest(self, files):
        entries = []
        for name, data in files.items():
            path = self.folder/name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            entries.append({'path': name, 'sha256': hashlib.sha256(data).hexdigest()})
        (self.folder/'publication-manifest.json').write_text(json.dumps({'files': entries}))

    def checked(self):
        with contextlib.redirect_stdout(io.StringIO()):
            module.check(self.folder)

    def test_clean_snapshot(self):
        self.manifest({'README.md': b'Project source\n'})
        self.checked()

    def test_source_tampering(self):
        self.manifest({'README.md': b'Expected\n'})
        (self.folder/'README.md').write_text('Changed\n')
        with self.assertRaises(SystemExit):
            self.checked()

    def test_original_game_payload(self):
        self.manifest({'README.md': b'Source\n'})
        (self.folder/'MEGARACE2.gog').write_bytes(b'not a real game image')
        with self.assertRaises(SystemExit):
            self.checked()

    def test_secret_even_if_hash_matches(self):
        self.manifest({'README.md': ('ghp_'+'A'*36).encode()})
        with self.assertRaises(SystemExit):
            self.checked()

    def test_private_home_even_if_hash_matches(self):
        self.manifest({'README.md': b'C:'+b'/'+b'Users/Example/private-file'})
        with self.assertRaises(SystemExit):
            self.checked()

    def test_traversal_manifest(self):
        (self.folder/'publication-manifest.json').write_text(json.dumps({
            'files': [{'path': '../outside', 'sha256': 'unused'}]}))
        with self.assertRaises(SystemExit):
            self.checked()

    def test_missing_source(self):
        self.manifest({'README.md': b'Source\n'})
        (self.folder/'README.md').unlink()
        with self.assertRaises(SystemExit):
            self.checked()

    def test_generated_build_products_not_in_inventory(self):
        self.manifest({'README.md': b'Source\n'})
        (self.folder/'build').mkdir()
        (self.folder/'build/OpenMR2.exe').write_bytes(b'build output')
        (self.folder/'winlib').mkdir()
        (self.folder/'winlib/gdi32.lib').write_bytes(b'generated import library')
        self.checked()

    def test_consolidation_keeps_sections_links_and_nested_images(self):
        result = module.consolidate_documents({
            'README.md': '# Project\n[Settings](CONFIGURATION.md#advanced)\n',
            'CONFIGURATION.md': '# Settings\n## Advanced\nKeep this setting.\n[Art](assets/art/README.md)\n',
            'assets/art/README.md': '# Artwork\n![Icon](icon.png)\n[Settings](../../CONFIGURATION.md)\n',
        })
        self.assertEqual(set(result), {'README.md', 'DOCUMENTATION.md'})
        self.assertIn('DOCUMENTATION.md#configuration--advanced', result['README.md'])
        manual = result['DOCUMENTATION.md']
        self.assertIn('id="configuration--advanced"', manual)
        self.assertIn('Keep this setting.', manual)
        self.assertIn('](#assets-art-readme)', manual)
        self.assertIn('![Icon](assets/art/icon.png)', manual)
        self.assertIn('](#configuration)', manual)

    def test_consolidation_preserves_fenced_examples(self):
        example = '```sh\n# Keep this comment\n[example](OLD.md)\n```\n'
        result = module.consolidate_documents({'README.md': '# Project\n',
                                               'BUILDING.md': '# Build\n'+example})
        self.assertIn(example, result['DOCUMENTATION.md'])

    def test_existing_consolidated_snapshot_is_stable(self):
        files = {'README.md': '# Project\n', 'DOCUMENTATION.md': '# Manual\n'}
        self.assertEqual(module.consolidate_documents(files), files)

    def test_public_layout_rejects_extra_markdown(self):
        self.manifest({'README.md': b'Project\n', 'DOCUMENTATION.md': b'Manual\n',
                       'nested/NOTES.md': b'Notes\n'})
        path = self.folder/'publication-manifest.json'
        data = json.loads(path.read_text())
        data['documentation'] = {'format': 'single-manual'}
        path.write_text(json.dumps(data))
        with self.assertRaises(SystemExit):
            self.checked()


if __name__ == '__main__':
    unittest.main()
