"""lwIP must be built for two SNTP servers, so pool.ntp.org gets a second one."""
import configparser
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
KEY = "CONFIG_LWIP_SNTP_MAX_SERVERS"
MINIMUMS = ROOT / "scripts" / "sdkconfig_minimums.cmake"


def environment_defaults():
    config = configparser.ConfigParser(interpolation=None)
    config.read(ROOT / "platformio.ini")
    common_args = config.get("env", "board_build.cmake_extra_args", fallback="")
    for name in config:
        if not name.startswith("env:"):
            continue
        defaults = ["sdkconfig.defaults"]
        for arg in shlex.split(config.get(name, "board_build.cmake_extra_args",
                                          fallback=common_args)):
            if arg.startswith("-DSDKCONFIG_DEFAULTS="):
                defaults = arg.split("=", 1)[1].split(";")
        yield name, defaults


class NtpSdkconfigTest(unittest.TestCase):
    def test_every_environment_builds_two_sntp_servers(self):
        environments = list(environment_defaults())
        self.assertTrue(environments)
        for environment, defaults in environments:
            with self.subTest(environment=environment, defaults=defaults):
                value = None
                for name in defaults:  # later files win, as in ESP-IDF
                    for line in (ROOT / name).read_text(encoding="utf-8").splitlines():
                        if line.strip().startswith(KEY + "="):
                            value = line.strip().split("=", 1)[1]
                self.assertEqual(value, "2")

    def test_existing_sdkconfig_is_raised_to_two_servers(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "sdkconfig.defaults").write_text(f"{KEY}=2\n")
            config = root / "sdkconfig"
            config.write_text(f"CONFIG_LOCAL_SETTING=42\n{KEY}=1\n")
            script = root / "minimums.cmake"
            script.write_text(f'set(CMAKE_SOURCE_DIR "{root.as_posix()}")\n'
                              f'include("{MINIMUMS.as_posix()}")\n'
                              "solar_os_apply_sdkconfig_minimums()\n")
            result = subprocess.run(["cmake", "-P", str(script)], cwd=root,
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            contents = config.read_text()
            self.assertIn("CONFIG_LOCAL_SETTING=42\n", contents)
            self.assertIn(f"{KEY}=2\n", contents)
            self.assertNotIn(f"{KEY}=1\n", contents)


if __name__ == "__main__":
    unittest.main()
