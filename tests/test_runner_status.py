import pathlib
import subprocess
import sys
import tempfile
import unittest

RUNNER = pathlib.Path(__file__).with_name("run_tests.py").resolve()


class RunnerStatusTests(unittest.TestCase):
    def run_fixture(self, *, tools=False, program=False, assembler_status=0):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            if tools:
                for name, status in [("assembler_bin", assembler_status), ("linker_bin", 0)]:
                    executable = root / name
                    executable.write_text(f"#!/bin/sh\nexit {status}\n")
                    executable.chmod(0o755)
            if program:
                source = root / "tests/test_programs/sample.s"
                source.parent.mkdir(parents=True)
                source.write_text("nop\n")
            return subprocess.run([sys.executable, str(RUNNER)], cwd=root, capture_output=True).returncode

    def test_missing_tools_fail(self):
        self.assertNotEqual(self.run_fixture(), 0)

    def test_missing_programs_fail(self):
        self.assertNotEqual(self.run_fixture(tools=True), 0)

    def test_failed_assembler_fails_run(self):
        self.assertNotEqual(self.run_fixture(tools=True, program=True, assembler_status=1), 0)

    def test_successful_program_succeeds(self):
        self.assertEqual(self.run_fixture(tools=True, program=True), 0)
