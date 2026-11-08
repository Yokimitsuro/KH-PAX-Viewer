"""Copy the effect player from a khdays-port checkout (its assets/src/kh2_effect.cpp and its header) into
src/player/, so the viewer plays effects exactly as the port does.

    python tools/sync_player.py <khdays-port checkout>
"""

import shutil
import sys
from pathlib import Path

FILES = {
    "assets/src/kh2_effect.cpp": "src/player/kh2_effect.cpp",
    "assets/include/khdays/assets/kh2_effect.h": "src/player/include/khdays/assets/kh2_effect.h",
}


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(__doc__)
        return 2
    port, here = Path(argv[1]), Path(__file__).resolve().parents[1]
    for src, dst in FILES.items():
        if not (port / src).is_file():
            print(f"sync_player: {port / src} is not there")
            return 1
    for src, dst in FILES.items():
        shutil.copyfile(port / src, here / dst)
        print(f"sync_player: {dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
