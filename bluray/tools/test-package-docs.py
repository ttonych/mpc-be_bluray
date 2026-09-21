"""Check the real packaged manuals, Cyrillic anchors and broken-link rejection."""
from pathlib import Path
import unittest
from package_docs import add_documents, rewrite_links, verify_links


ROOT = Path(__file__).resolve().parents[2]
COMMIT = '0' * 40


class PackageDocsTests(unittest.TestCase):
    def test_real_manuals(self):
        data = {'licenses/MPC-BE-License.txt': b'license'}
        add_documents(data, ROOT, COMMIT)
        self.assertIn(b'lang="ru"', data['Readme.ru.html'])
        self.assertIn(b'docs/ru/USAGE.html#', data['Readme.ru.html'])
        self.assertIn(('id="java-для-bd-j"').encode(), data['docs/ru/USAGE.html'])
        self.assertNotIn(b'<script', b''.join(data.values()))
        self.assertIn(f'/blob/{COMMIT}/docs/en/DEVELOPMENT.md'.encode(), data['Readme.html'])
        self.assertIn(b'../../Readme.html', data['docs/en/USAGE.html'])

    def test_broken_file_and_anchor(self):
        with self.assertRaisesRegex(ValueError, 'missing packaged link'):
            verify_links({'Readme.html': b'<a href="absent.html">link</a>'})
        with self.assertRaisesRegex(ValueError, 'missing packaged anchor'):
            verify_links({'Readme.html': b'<a href="#absent">link</a>'})

    def test_source_escape(self):
        with self.assertRaisesRegex(ValueError, 'invalid document link'):
            rewrite_links('[bad](../outside.md)', 'README.md', 'Readme.html', ROOT, COMMIT)


if __name__ == '__main__':
    unittest.main()
