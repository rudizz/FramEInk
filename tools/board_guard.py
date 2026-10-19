#!/usr/bin/env python3
"""Check the actual ESP32 MAC and firmware target before an Inkplate upload.

The Arduino CLI adapter installs this check in the final esptool recipe, so
compile --upload checks the device after compilation, immediately before flash.
"""

import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
REGISTRY = ROOT / ".vscode/board-identities.local.json"
MODELS = {"Inkplate10": "FramEink", "Inkplate6COLOR": "FramEink_6Color"}
TAGS = {model: ("FRAMEINK_BOARD_V1:" + model + "\0").encode() for model in MODELS}
MAC = re.compile(r"(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}\Z")


class GuardError(Exception):
    pass


def real_cli():
    configured = os.environ.get("FRAMEINK_ARDUINO_CLI")
    candidates = [configured] if configured else [
        "/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli",
        shutil.which("arduino-cli"),
    ]
    wrapper = ROOT / ".vscode/arduino-cli-wrapper/bin/arduino-cli"
    for candidate in candidates:
        if candidate and Path(candidate).is_file() and Path(candidate).resolve() != wrapper.resolve():
            return str(Path(candidate).resolve())
    raise GuardError("Arduino CLI non trovato. Imposta FRAMEINK_ARDUINO_CLI al suo eseguibile reale.")


def option(arguments, *names):
    values = []
    for index, argument in enumerate(arguments):
        for name in names:
            if argument == name:
                if index + 1 >= len(arguments) or arguments[index + 1].startswith("-"):
                    raise GuardError("Valore mancante per " + name)
                values.append(arguments[index + 1])
            elif argument.startswith(name + "="):
                values.append(argument[len(name) + 1:])
    if len(set(values)) > 1:
        raise GuardError("Opzioni in conflitto: " + "/".join(names))
    return values[-1] if values else None


def model_from_fqbn(fqbn):
    fields = (fqbn or "").split(":")
    if len(fields) < 3 or fields[:2] != ["Inkplate_Boards", "esp32"] or fields[2] not in MODELS:
        raise GuardError("Upload bloccato: specifica il FQBN Inkplate10 o Inkplate6COLOR.")
    return fields[2]


def load_registry(path=None):
    path = REGISTRY if path is None else path
    if not path.exists():
        return {}
    try:
        document = json.loads(path.read_text())
        devices = document["devices"]
        if document["version"] != 1 or not isinstance(devices, dict):
            raise ValueError("formato non valido")
        if any(not MAC.fullmatch(mac) or mac != mac.lower() or model not in MODELS
               for mac, model in devices.items()):
            raise ValueError("MAC o modello non valido")
        return devices
    except (ValueError, KeyError, TypeError, OSError) as error:
        raise GuardError("Registro schede non valido: " + str(path)) from error


def tool_command(path):
    return [sys.executable, str(path)] if str(path).endswith(".py") else [str(path)]


def read_mac(esptool, port):
    # This only reads eFuses. Never erase/write flash to identify a device.
    command = tool_command(esptool) + ["--chip", "esp32", "--port", port, "--baud", "115200",
        "--no-stub", "--before", "default_reset", "--after", "hard_reset", "read_mac"]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=35)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise GuardError("Impossibile leggere la scheda sulla porta " + port + ". Upload bloccato.") from error
    if result.returncode:
        raise GuardError("Lettura MAC fallita; nessun upload eseguito.\n" + result.stdout + result.stderr)
    addresses = set(re.findall(r"(?im)^\s*MAC:\s*((?:[0-9a-f]{2}:){5}[0-9a-f]{2})(?=\s|$)", result.stdout))
    if len(addresses) != 1:
        raise GuardError("MAC della scheda assente o ambiguo. Upload bloccato.")
    address = addresses.pop().lower()
    if address in {"00:00:00:00:00:00", "ff:ff:ff:ff:ff:ff"}:
        raise GuardError("MAC della scheda non valido. Upload bloccato.")
    return address


def verify_device(model, address, devices):
    connected = devices.get(address)
    if connected is None:
        raise GuardError("Scheda " + address + " non registrata. Upload bloccato.\n"
            "Controlla fisicamente il modello e registrala una volta con:\n"
            "python3 tools/board_guard.py register --model " + model + " --port PORTA")
    if connected != model:
        raise GuardError("Scheda collegata: " + connected + " (" + address + "). Firmware selezionato: " +
            model + ". UPLOAD BLOCCATO: collega la scheda corretta.")


def firmware_file(arguments):
    commands = [i for i, value in enumerate(arguments) if value in {"write_flash", "write-flash"}]
    if len(commands) != 1:
        raise GuardError("Operazione di scrittura non riconosciuta. Upload bloccato.")
    files = []
    for index in range(commands[0] + 1, len(arguments) - 1):
        if arguments[index].lower() in {"0x10000", "65536"}:
            files.append(Path(arguments[index + 1]))
    if len(files) != 1 or not files[0].is_file():
        raise GuardError("Firmware applicativo a 0x10000 non identificato. Upload bloccato.")
    return files[0]


def verify_firmware(model, arguments):
    path = firmware_file(arguments)
    try:
        with path.open("rb") as source:
            data = source.read(4 * 1024 * 1024 + 1)
    except OSError as error:
        raise GuardError("Firmware non leggibile. Upload bloccato.") from error
    matches = [target for target, tag in TAGS.items() if tag in data]
    if not data or data[0] != 0xE9 or len(data) > 4 * 1024 * 1024 or matches != [model]:
        raise GuardError("Il file " + path.name + " non identifica un firmware " + model +
            ". Upload bloccato: ricompila lo sketch corretto.")


def flash(model, esptool, arguments, registry=None):
    port = option(arguments, "--port", "-p")
    if not port or option(arguments, "--chip", "-c") != "esp32":
        raise GuardError("Porta seriale o chip ESP32 non specificati. Upload bloccato.")
    if "--force" in arguments or any("encrypt" in argument for argument in arguments):
        raise GuardError("Opzioni di scrittura non supportate dall'upload protetto.")
    verify_firmware(model, arguments)
    devices = load_registry(registry)
    address = read_mac(esptool, port)
    verify_device(model, address, devices)
    print("[Board guard] " + model + " verificata: " + address + ". Avvio upload.", file=sys.stderr, flush=True)
    result = subprocess.call(tool_command(esptool) + arguments)
    if result == 0:
        print("[Board guard] Upload completato correttamente su " + model + " (" + address + ").",
              file=sys.stderr, flush=True)
    return result


def recipe(model):
    # Arduino expands the tool-specific {path}, {cmd} and {upload.pattern_args}.
    return ('"' + sys.executable + '" "' + str(Path(__file__).resolve()) +
        '" flash --model ' + model + ' --esptool "{path}/{cmd}" -- {upload.pattern_args}')


def is_upload(arguments):
    if "upload" in arguments:
        return True
    if "compile" not in arguments:
        return False
    enabled = False
    for value in arguments:
        if value in {"--upload", "-u"}:
            enabled = True
        elif value.startswith(("--upload=", "-u=")):
            setting = value.split("=", 1)[1]
            if setting in {"true", "True", "TRUE", "t", "T", "1"}:
                enabled = True
            elif setting in {"false", "False", "FALSE", "f", "F", "0"}:
                enabled = False
            else:
                raise GuardError("Valore booleano non valido per --upload.")
    return enabled


def protected_cli_args(arguments):
    if "compile" in arguments and any(value.startswith("-") and not value.startswith("--") and len(value) > 2
                                      for value in arguments):
        raise GuardError("Usa opzioni brevi separate (-u -v), oppure opzioni lunghe (--upload --verbose).")
    if not is_upload(arguments):
        return list(arguments)
    if any(value in {"--help", "-h"} for value in arguments):
        return list(arguments)
    if "compile" in arguments and any(value == name or value.startswith(name + "=")
            for value in arguments for name in ("--show-properties", "--preprocess", "--only-compilation-database")):
        raise GuardError("Le modalità di sola analisi/anteprima non possono essere combinate con --upload.")
    model = model_from_fqbn(option(arguments, "--fqbn", "-b"))
    if option(arguments, "--programmer", "-P") or option(arguments, "--profile", "-m"):
        raise GuardError("Programmatore/profilo non supportato dall'upload protetto. Usa FQBN e porta seriale.")
    port = option(arguments, "--port", "-p")
    if not port:
        raise GuardError("Specificare la porta seriale della scheda. Upload bloccato.")
    if not port.startswith("/dev/") and not re.fullmatch(r"COM[1-9][0-9]*", port, re.IGNORECASE):
        raise GuardError("L'upload protetto richiede una porta seriale locale /dev/... o COM... .")
    protocol = option(arguments, "--protocol", "-l")
    if protocol and protocol != "serial":
        raise GuardError("L'upload protetto supporta solo la connessione seriale USB.")
    # Do not allow an alternate upload recipe to bypass the check.
    if any("upload." in value or "esptool_py.cmd=" in value or "esptool_py.path=" in value
           for value in arguments):
        raise GuardError("Override della procedura di upload non consentito.")
    if "compile" in arguments:
        return list(arguments) # cli_main splits compile/upload: CLI ignores build-only recipe overrides on upload.
    result = list(arguments)
    pattern = recipe(model)
    for suffix in ("", ".linux", ".macosx", ".windows"):
        result += ["--upload-property", "upload.pattern" + suffix + "=" + pattern]
    # OTA and programmer routes must never fall back to an unguarded recipe.
    result += ["--upload-property", "upload.network_pattern=" + pattern]
    return result


def cli_main(arguments=None):
    try:
        original = list(sys.argv[1:] if arguments is None else arguments)
        arguments = protected_cli_args(original)
        cli = real_cli()
        if is_upload(original) and not any(value in {"--help", "-h"} for value in original):
            print("[Board guard] Upload protetto attivo: il MAC sarà verificato prima della scrittura.",
                  file=sys.stderr, flush=True)
        if "compile" in original and is_upload(original) and not any(value in {"--help", "-h"} for value in original):
            fqbn = option(original, "--fqbn", "-b")
            model = model_from_fqbn(fqbn)
            build_path = option(original, "--build-path")
            if not build_path:
                build_path = str(ROOT / ".build" / ("inkplate10" if model == "Inkplate10" else "inkplate6color"))
                arguments += ["--build-path", build_path]
            arguments = [value for value in arguments if value not in {"--upload", "-u"} and
                         not value.startswith(("--upload=", "-u="))]
            compiled = subprocess.call([cli] + arguments)
            if compiled:
                return compiled
            # Use the actual compiled sketch and build directory, including VS Code's --build-path.
            metadata = json.loads((Path(build_path) / "build.options.json").read_text())
            sketch = metadata["sketchLocation"]
            if not isinstance(sketch, str) or not Path(sketch).is_dir():
                raise GuardError("Sketch compilato non identificato; upload bloccato.")
            upload = ["upload", "--fqbn", fqbn, "--port", option(original, "--port", "-p"),
                      "--input-dir", str(Path(build_path).resolve()), sketch]
            if option(original, "--protocol", "-l"):
                upload += ["--protocol", "serial"]
            for name in ("--config-file", "--config-dir", "--additional-urls", "--board-options", "--discovery-timeout"):
                for index, value in enumerate(original):
                    if value == name:
                        upload += [name, original[index + 1]]
                    elif value.startswith(name + "="):
                        upload.append(value)
            for name in ("--verify", "-t", "--verbose", "-v", "--json", "--no-color"):
                if name in original:
                    upload.append(name)
            return subprocess.call([cli] + protected_cli_args(upload))
        return subprocess.call([cli] + arguments)
    except (GuardError, OSError, ValueError, KeyError, TypeError) as error:
        print("[Board guard] " + str(error), file=sys.stderr)
        return 2


def installed_esptool(model):
    # VS Code's pre-build hook passes only VSCA_* variables, not the user's
    # environment. Restore the actual user's home for CLI data discovery.
    environment = os.environ.copy()
    environment.setdefault("HOME", str(Path.home()))
    environment.setdefault("PATH", os.defpath)
    with tempfile.TemporaryDirectory(prefix="frameink-board-properties-") as folder:
        result = subprocess.run([real_cli(), "compile", "--fqbn", "Inkplate_Boards:esp32:" + model,
            "--build-path", folder, "--show-properties=expanded", str(ROOT / MODELS[model])],
            capture_output=True, text=True, timeout=60, env=environment)
    if result.returncode:
        raise GuardError("Impossibile trovare esptool tramite Arduino CLI.\n" + result.stderr)
    properties = dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)
    path = properties.get("tools.esptool_py.path", "")
    command = properties.get("tools.esptool_py.cmd", "")
    if not path or not command or not Path(path, command).is_file():
        raise GuardError("Eseguibile esptool non trovato. Usa --esptool PERCORSO.")
    return str(Path(path, command))


def vscode_preflight():
    """Fail before compilation even if the extension has cached an old CLI path."""
    mode = os.environ.get("VSCA_BUILD_MODE", "")
    if not mode:
        raise GuardError("Contesto VS Code assente: controllo pre-upload non eseguito.")
    if not mode.startswith("Uploading"):
        return 0
    if "programmer" in mode.lower():
        raise GuardError("Programmatore esterno non supportato dall'upload protetto.")
    model = model_from_fqbn(os.environ.get("VSCA_BOARD"))
    port = os.environ.get("VSCA_SERIAL")
    if not port:
        raise GuardError("Porta della scheda assente: upload bloccato.")
    devices = load_registry()
    print("[Board guard] Controllo della scheda collegata prima della compilazione...", file=sys.stderr, flush=True)
    address = read_mac(installed_esptool(model), port)
    verify_device(model, address, devices)
    print("[Board guard] Scheda registrata verificata: " + model + " (" + address + ").",
          file=sys.stderr, flush=True)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    register = sub.add_parser("register", help="Associa il MAC al modello verificato fisicamente; non carica firmware.")
    register.add_argument("--model", choices=MODELS, required=True)
    register.add_argument("--port", required=True)
    register.add_argument("--esptool")
    sub.add_parser("vscode-preflight", help="Controllo preventivo eseguito dal pre-build di VS Code.")
    flasher = sub.add_parser("flash", help=argparse.SUPPRESS)
    flasher.add_argument("--model", choices=MODELS, required=True)
    flasher.add_argument("--esptool", required=True)
    flasher.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        if args.action == "vscode-preflight":
            return vscode_preflight()
        if args.action == "flash":
            arguments = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
            return flash(args.model, args.esptool, arguments)
        devices = load_registry()
        address = read_mac(args.esptool or installed_esptool(args.model), args.port)
        if address in devices and devices[address] != args.model:
            raise GuardError("Questo MAC è già registrato come " + devices[address] + ". Nessuna modifica eseguita.")
        devices[address] = args.model
        REGISTRY.parent.mkdir(parents=True, exist_ok=True)
        # Write atomically, preserving all other registered devices.
        with tempfile.NamedTemporaryFile(mode="w", dir=REGISTRY.parent, delete=False) as output:
            json.dump({"version": 1, "devices": devices}, output, indent=2, sort_keys=True)
            output.write("\n")
            temporary = Path(output.name)
        try:
            os.replace(temporary, REGISTRY)
        finally:
            temporary.unlink(missing_ok=True)
        print("Registrata " + args.model + ": " + address + ". Nessun firmware caricato.")
        return 0
    except (GuardError, OSError, subprocess.TimeoutExpired) as error:
        print("[Board guard] " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
