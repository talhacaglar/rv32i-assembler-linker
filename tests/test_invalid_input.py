import pathlib
import subprocess
import tempfile
import unittest

ASSEMBLER = pathlib.Path(__file__).resolve().parents[1] / "assembler_bin"


class InvalidInputTests(unittest.TestCase):
    def test_invalid_instructions_fail_without_output(self):
        for instruction in ["add x1", "addi x1,x2", "add x1,x2,x3,x4,x5,x6,x7", "nonexistent x1"]:
            with self.subTest(instruction=instruction), tempfile.TemporaryDirectory() as directory:
                source = pathlib.Path(directory) / "program.s"
                source.write_text(instruction + "\n")
                result = subprocess.run([str(ASSEMBLER), str(source)], capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"[ERR]", result.stderr)
                self.assertFalse(source.with_suffix(".o").exists())

    def test_missing_source_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run([str(ASSEMBLER), str(pathlib.Path(directory) / "missing.s")], capture_output=True)
            self.assertNotEqual(result.returncode, 0)

    def test_signed_zero_branch_pseudoinstructions_are_supported(self):
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / "program.s"
            source.write_text("bltz x1,4\nbgtz x2,4\n")
            result = subprocess.run([str(ASSEMBLER), str(source)], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertTrue(source.with_suffix(".o").exists())
