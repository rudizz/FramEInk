"""Upload contract tests with no serial devices and no flash writes."""
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import board_guard as guard


class GuardTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="frameink-upload-test-")
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.binary = self.folder / "firmware with spaces.bin"
        self.binary.write_bytes(b"\xe9" + b"\x00" * 31 + guard.TAGS["Inkplate10"])
        self.registry = self.folder / "devices.json"
        self.registry.write_text(json.dumps({"version": 1, "devices": {"aa:bb:cc:dd:ee:02": "Inkplate10",
                                                                              "aa:bb:cc:dd:ee:06": "Inkplate6COLOR"}}))
        self.arguments = ["--chip", "esp32", "--port", "/dev/serial-test", "--baud", "460800", "write_flash",
                          "-z", "0x1000", "bootloader.bin", "0x10000", str(self.binary), "0x8000", "partitions.bin"]

    def test_only_matching_physical_board_can_write(self):
        for address, allowed in [("aa:bb:cc:dd:ee:02", True), ("aa:bb:cc:dd:ee:06", False),
                                 ("aa:bb:cc:dd:ee:ff", False)]:
            with self.subTest(address=address), patch.object(guard, "read_mac", return_value=address), \
                 patch.object(guard.subprocess, "call", return_value=0) as writer, \
                 contextlib.redirect_stdout(io.StringIO()):
                if allowed:
                    self.assertEqual(guard.flash("Inkplate10", "/tool/esptool", self.arguments, self.registry), 0)
                    writer.assert_called_once_with(["/tool/esptool"] + self.arguments)
                else:
                    with self.assertRaises(guard.GuardError):
                        guard.flash("Inkplate10", "/tool/esptool", self.arguments, self.registry)
                    writer.assert_not_called()

    def test_wrong_or_old_binary_is_rejected_before_serial(self):
        for content in [b"\xe9" + guard.TAGS["Inkplate6COLOR"], b"\xe9 old firmware", b"invalid", 
                        b"\xe9" + b"".join(guard.TAGS.values())]:
            with self.subTest(content=content), patch.object(guard, "read_mac") as reader, \
                 patch.object(guard.subprocess, "call") as writer:
                self.binary.write_bytes(content)
                with self.assertRaises(guard.GuardError):
                    guard.flash("Inkplate10", "/tool/esptool", self.arguments, self.registry)
                reader.assert_not_called()
                writer.assert_not_called()

    def test_missing_invalid_registry_and_read_errors_never_write(self):
        for document in ["{}", '{"version": 1, "devices": {"broken": "Inkplate10"}}',
                         '{"version": 2, "devices": {}}', "[1]", "invalid"]:
            self.registry.write_text(document)
            with patch.object(guard.subprocess, "call") as writer:
                with self.assertRaises(guard.GuardError):
                    guard.flash("Inkplate10", "/tool/esptool", self.arguments, self.registry)
                writer.assert_not_called()
        self.registry.unlink()
        with patch.object(guard, "read_mac", return_value="aa:bb:cc:dd:ee:02"), \
             patch.object(guard.subprocess, "call") as writer:
            with self.assertRaises(guard.GuardError):
                guard.flash("Inkplate10", "/tool/esptool", self.arguments, self.registry)
            writer.assert_not_called()

    def test_mac_probe_is_read_only_and_bounded(self):
        completed = subprocess.CompletedProcess([], 0, "Chip is ESP32\nMAC: AA:BB:CC:DD:EE:02\n", "")
        with patch.object(guard.subprocess, "run", return_value=completed) as runner:
            self.assertEqual(guard.read_mac("/tool/esptool", "/dev/serial-test"), "aa:bb:cc:dd:ee:02")
            command = runner.call_args.args[0]
            self.assertEqual(command[-1], "read_mac")
            self.assertIn("--no-stub", command)
            self.assertNotIn("write_flash", command)
            self.assertNotIn("erase_flash", command)
            self.assertEqual(runner.call_args.kwargs["timeout"], 35)
        for output in ["", "MAC: 00:00:00:00:00:00", "MAC: aa:bb:cc:dd:ee:02\nMAC: aa:bb:cc:dd:ee:06"]:
            with patch.object(guard.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, output, "")):
                with self.assertRaises(guard.GuardError):
                    guard.read_mac("/tool/esptool", "/dev/serial-test")
        for error in [OSError("busy"), subprocess.TimeoutExpired([], 35)]:
            with patch.object(guard.subprocess, "run", side_effect=error):
                with self.assertRaises(guard.GuardError):
                    guard.read_mac("/tool/esptool", "/dev/serial-test")

    def test_cli_injects_final_upload_guard(self):
        args = ["upload", "-b", "Inkplate_Boards:esp32:Inkplate10:PartitionScheme=huge_app",
                "--port=/dev/serial-test", "FramEink"]
        protected = guard.protected_cli_args(args)
        recipe = next(value for value in protected if value.startswith("upload.pattern="))
        self.assertIn("board_guard.py", recipe)
        self.assertIn("flash --model Inkplate10", recipe)
        self.assertIn('"{path}/{cmd}"', recipe)
        self.assertIn("{upload.pattern_args}", recipe)
        self.assertEqual(protected[:len(args)], args)

    def test_compile_upload_is_split_and_only_upload_has_guard_recipe(self):
        (self.folder / "build.options.json").write_text(json.dumps({"sketchLocation": str(self.folder)}))
        for switches in [["--upload"], ["-u"], ["--upload=true"], ["--upload=1"]]:
            args = ["compile"] + switches + ["--fqbn", "Inkplate_Boards:esp32:Inkplate10", "--port", "/dev/serial-test",
                "--build-path", str(self.folder), "--config-file=test.yaml", "--verify", "FramEink"]
            with patch.object(guard, "real_cli", return_value="/real/arduino-cli"), \
                 patch.object(guard.subprocess, "call", side_effect=[0, 0]) as child:
                self.assertEqual(guard.cli_main(args), 0)
                self.assertEqual(child.call_count, 2)
                compile_args = child.call_args_list[0].args[0]
                self.assertIn("compile", compile_args)
                self.assertNotIn(switches[0], compile_args)
                upload_args = child.call_args_list[1].args[0]
                self.assertIn("upload", upload_args)
                self.assertIn("--verify", upload_args)
                self.assertIn("--config-file=test.yaml", upload_args)
                self.assertIn(str(self.folder), upload_args)
                self.assertTrue(any(value.startswith("upload.pattern=") for value in upload_args))
        with patch.object(guard, "real_cli", return_value="/real/arduino-cli"), \
             patch.object(guard.subprocess, "call", return_value=17) as child:
            self.assertEqual(guard.cli_main(args), 17)
            self.assertEqual(child.call_count, 1)

    def test_ordinary_compile_and_queries_pass_through(self):
        for args in [["version"], ["board", "list", "--json"], ["compile", "FramEink"],
                     ["compile", "--upload=false", "FramEink"], ["compile", "--upload", "--upload=false", "FramEink"], ["upload", "--help"]]:
            self.assertEqual(guard.protected_cli_args(args), args)

    def test_no_unguarded_alternative_upload(self):
        base = ["upload", "-b", "Inkplate_Boards:esp32:Inkplate10", "-p", "/dev/serial-test"]
        for extra in [["--programmer", "test"], ["--profile", "test"], ["--protocol", "network"],
                      ["--upload-property", "upload.pattern=unsafe"], ["--port", "/dev/another-port"]]:
            with self.subTest(extra=extra), self.assertRaises(guard.GuardError):
                guard.protected_cli_args(base + extra)
        for args in [["upload"], ["compile", "--upload=maybe"], ["upload", "-b", "esp32:esp32:esp32"],
                     ["compile", "--upload", "--show-properties"], ["compile", "--upload", "--preprocess"],
                     ["compile", "--upload", "--only-compilation-database"],
                     ["compile", "-uv"], ["compile", "--upload", "-b", "Inkplate_Boards:esp32:Inkplate10"],
                     ["upload", "-b", "Inkplate_Boards:esp32:Inkplate10", "-p", "192.168.1.2"]]:
            with self.subTest(args=args), self.assertRaises(guard.GuardError):
                guard.protected_cli_args(args)

    def test_cli_failure_is_propagated(self):
        with patch.object(guard, "real_cli", return_value="/real/arduino-cli"), \
             patch.object(guard.subprocess, "call", return_value=12):
            self.assertEqual(guard.cli_main(["version"]), 12)
        with patch.object(guard.subprocess, "call") as child, contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(guard.cli_main(["upload"]), 2)
            child.assert_not_called()

    def test_registration_saves_mac_without_upload_and_rejects_reassignment(self):
        for model, success in [("Inkplate10", True), ("Inkplate6COLOR", False)]:
            args = ["board_guard.py", "register", "--model", model, "--port", "/dev/serial-test", "--esptool", "/tool/esptool"]
            with patch.object(guard, "REGISTRY", self.registry), patch.object(guard, "read_mac", return_value="aa:bb:cc:dd:ee:02"), \
                 patch.object(guard.subprocess, "call") as writer, patch.object(sys, "argv", args), \
                 contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(guard.main(), 0 if success else 2)
                writer.assert_not_called()
                self.assertEqual(json.loads(self.registry.read_text())["devices"]["aa:bb:cc:dd:ee:02"], "Inkplate10")

    def test_vscode_prebuild_blocks_unknown_and_wrong_boards_without_writing(self):
        for mode in ["Uploading", "Uploading using Arduino CLI"]:
            environment = {"VSCA_BUILD_MODE": mode, "VSCA_BOARD": "Inkplate_Boards:esp32:Inkplate10",
                           "VSCA_SERIAL": "/dev/serial-test"}
            for address, allowed in [("aa:bb:cc:dd:ee:02", True), ("aa:bb:cc:dd:ee:06", False),
                                     ("aa:bb:cc:dd:ee:ff", False)]:
                with self.subTest(mode=mode, address=address), patch.dict(os.environ, environment, clear=True), \
                     patch.object(guard, "REGISTRY", self.registry), patch.object(guard, "read_mac", return_value=address), \
                     patch.object(guard, "installed_esptool", return_value="/tool/esptool"), \
                     patch.object(guard.subprocess, "call") as writer, contextlib.redirect_stderr(io.StringIO()) as messages:
                    if allowed:
                        self.assertEqual(guard.vscode_preflight(), 0)
                        self.assertIn("Scheda registrata verificata", messages.getvalue())
                    else:
                        with self.assertRaises(guard.GuardError):
                            guard.vscode_preflight()
                    writer.assert_not_called()

    def test_vscode_analysis_never_connects_to_device(self):
        for mode in ["Analyzing", "Verifying"]:
            with patch.dict(os.environ, {"VSCA_BUILD_MODE": mode}, clear=True), patch.object(guard, "read_mac") as reader:
                self.assertEqual(guard.vscode_preflight(), 0)
                reader.assert_not_called()
        with patch.dict(os.environ, {}, clear=True), self.assertRaises(guard.GuardError):
            guard.vscode_preflight()

    def test_vscode_minimal_hook_environment_can_find_cli_tools(self):
        tool = self.folder / "esptool"
        tool.touch()
        result = subprocess.CompletedProcess([], 0, f"tools.esptool_py.path={self.folder}\ntools.esptool_py.cmd=esptool\n", "")
        with patch.dict(os.environ, {"VSCA_BUILD_MODE": "Uploading"}, clear=True), \
             patch.object(guard, "real_cli", return_value="/real/arduino-cli"), \
             patch.object(guard.subprocess, "run", return_value=result) as runner:
            self.assertEqual(guard.installed_esptool("Inkplate10"), str(tool))
            environment = runner.call_args.kwargs["env"]
            self.assertTrue(environment["HOME"])
            self.assertTrue(environment["PATH"])

    def test_upload_status_is_visible_to_vscode_and_only_completes_after_success(self):
        for status in [0, 1]:
            with patch.object(guard, "read_mac", return_value="aa:bb:cc:dd:ee:02"), \
                 patch.object(guard.subprocess, "call", return_value=status), \
                 contextlib.redirect_stderr(io.StringIO()) as messages:
                self.assertEqual(guard.flash("Inkplate10", "/tool/esptool", self.arguments, self.registry), status)
                self.assertIn("Avvio upload", messages.getvalue())
                self.assertEqual("Upload completato correttamente" in messages.getvalue(), status == 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
