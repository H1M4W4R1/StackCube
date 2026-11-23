"""Build and stage the StackCube games for a microSD card."""

import json
from pathlib import Path
import shutil
import subprocess
import sys
from uuid import uuid4


def game_folder(project):
    folders = [path for path in (project / "sd" / "games").glob("*") if path.is_dir()]
    if len(folders) != 1:
        raise ValueError(f"{project.name} must have one game folder under sd/games.")
    return folders[0]


def binary_name(folder):
    manifest = folder / ".gameinfo"
    if not manifest.is_file():
        manifests = [path for path in folder.glob("*.gameinfo") if path.is_file()]
        if len(manifests) != 1:
            raise ValueError(f"{folder.name} must have a .gameinfo manifest.")
        manifest = manifests[0]

    data = json.loads(manifest.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError(f"{manifest} must contain a JSON object.")
    name = data.get("binary", "game.bin")
    if (not isinstance(name, str) or not name.endswith(".bin") or name.startswith(".")
            or len(name.encode("utf-8")) > 120 or any(char in name for char in '/\\:\0')):
        raise ValueError(f"{manifest} must specify a plain .bin filename.")
    return name


def publish(staged, dist):
    previous = staged.parent / "previous"
    try:
        if dist.exists():
            dist.rename(previous)
        staged.rename(dist)
    except BaseException:
        if previous.exists() and not dist.exists():
            previous.rename(dist)
        raise


def prepare_sd(games_dir):
    pio = shutil.which("pio")
    if pio is None:
        raise RuntimeError("Install PlatformIO Core and make sure `pio` is on PATH.")

    projects = sorted(config.parent for config in games_dir.glob("*/platformio.ini"))
    if not projects:
        raise RuntimeError("No PlatformIO game projects found in Games.")

    games_dir = games_dir.resolve()
    dist = games_dir / "dist"
    # Let the staged package inherit the workspace permissions on Windows.
    work_dir = games_dir / f".dist-{uuid4().hex}"
    work_dir.mkdir()
    try:
        staged = work_dir / "dist"
        folders = set()
        for project in projects:
            source = game_folder(project)
            folder_name = source.name.casefold()
            if folder_name in folders:
                raise ValueError(f"Duplicate SD card game folder: {source.name}")
            folders.add(folder_name)
            destination = staged / "games" / source.name
            name = binary_name(source)

            print(f"Building {project.name}", flush=True)
            subprocess.run([pio, "run", "-d", str(project), "-e", "cores3"], check=True)
            firmware = project / ".pio" / "build" / "cores3" / "firmware.bin"
            shutil.copytree(source, destination)
            shutil.copyfile(firmware, destination / name)

        publish(staged, dist)
    finally:
        if not (work_dir / "previous").exists() or dist.exists():
            shutil.rmtree(work_dir)

    print(f"Ready: copy the contents of {dist} to the root of your FAT32 microSD card.")


if __name__ == "__main__":
    try:
        prepare_sd(Path(__file__).resolve().parent)
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"SD card preparation failed: {error}", file=sys.stderr)
        sys.exit(1)
