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

    def test_invalid_registers_fail_without_object_output(self):
        for instruction in ["add x32,x1,x2", "addi x1,nope,1", "lw x1,4(nope)", "sw nope,4(x1)"]:
            with self.subTest(instruction=instruction), tempfile.TemporaryDirectory() as directory:
                source = pathlib.Path(directory) / "program.s"
                source.write_text(instruction + "\n")
                result = subprocess.run([str(ASSEMBLER), str(source)], capture_output=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"[ERR]", result.stderr)
                self.assertFalse(source.with_suffix(".o").exists())

    def test_output_open_failure_is_not_success(self):
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / "program.s"
            source.write_text("nop\n")
            source.with_suffix(".o").mkdir()
            result = subprocess.run([str(ASSEMBLER), str(source)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"[ERR]", result.stderr)

    def test_linker_output_failure_is_not_success(self):
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / "program.s"
            source.write_text("nop\n")
            subprocess.run([str(ASSEMBLER), str(source)], check=True, capture_output=True)
            linker = ASSEMBLER.with_name("linker_bin")
            result = subprocess.run([str(linker), str(source.with_suffix(".o")),
                                     "-o", str(pathlib.Path(directory) / "missing" / "output")], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"[ERR]", result.stderr)

    @unittest.skipUnless(pathlib.Path("/dev/full").exists(), "requires Linux /dev/full")
    def test_buffered_output_failure_is_not_success(self):
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / "program.s"
            source.write_text("nop\n")
            source.with_suffix(".o").symlink_to("/dev/full")
            result = subprocess.run([str(ASSEMBLER), str(source)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"[ERR]", result.stderr)
