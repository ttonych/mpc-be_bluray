"""Regression checks for the publication scanner, with synthetic data only."""
import importlib.util
import contextlib
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('public_check', Path(__file__).with_name('check-public-tree.py'))
scanner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scanner)

class PublicCheckTests(unittest.TestCase):
    def test_normal_source(self):
        self.assertEqual(scanner.inspect('src/example.cpp', b'int answer = 42;'), [])

    def test_personal_path(self):
        text = ('C:' + '/Users/' + 'Example/profile').encode()
        self.assertTrue(scanner.inspect('docs/example.md', text))

    def test_personal_utf16_path(self):
        text = ('C:' + '\\Users\\' + 'Example\\profile').encode('utf-16')
        self.assertTrue(scanner.inspect('docs/example.md', text))

    def test_forced_tracked_profile(self):
        self.assertTrue(scanner.inspect('bluray/out/player/mpc-be64.ini', b'[Settings]'))

    def test_token(self):
        self.assertTrue(scanner.inspect('tools/example.py', ('ghp_' + 'x' * 36).encode()))

    def test_binary_disguised_as_source(self):
        self.assertTrue(scanner.inspect('tools/example.txt', b'MZ' + b'\0' * 100))

    def test_saved_disc_data(self):
        self.assertTrue(scanner.inspect('bdj-data/persistent/state', b'example'))

    def test_canvas_substitution(self):
        self.assertTrue(scanner.inspect(scanner.CANVAS, b'wrong content'))

    def test_secret_deleted_later_still_blocks_publication(self):
        with tempfile.TemporaryDirectory() as folder:
            repo = Path(folder)
            def git(*args):
                return subprocess.check_output(['git', '-C', folder, *args], stderr=subprocess.DEVNULL).decode().strip()
            git('init')
            git('config', 'user.name', 'Fixture')
            git('config', 'user.email', 'fixture@example.invalid')
            (repo / 'source.txt').write_text('ordinary source')
            git('add', 'source.txt')
            git('commit', '-m', 'base')
            base = git('rev-parse', 'HEAD')
            (repo / 'source.txt').write_text('ghp_' + 'x' * 36)
            git('commit', '-am', 'synthetic secret')
            (repo / 'source.txt').write_text('ordinary source')
            git('commit', '-am', 'remove synthetic secret')
            with patch.object(scanner, 'ROOT', repo), patch('sys.argv', ['check', '--base', base]), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(scanner.main(), 1)

if __name__ == '__main__':
    unittest.main()
