#!/usr/bin/env python3
"""Makes the manual: takes the pictures LeitnerTest wrote, puts them where the manual and the README want them,
and prints manual.html to "Leitner Manual.pdf" with Chrome.

    build/LeitnerTest_artefacts/Release/LeitnerTest /tmp/shots
    python3 docs/manual/make.py /tmp/shots

(needs Google Chrome or Chromium, and macOS's sips for the JPEGs; without a folder it only prints the PDF)"""
import os, shutil, subprocess, sys, tempfile

here = os.path.dirname(os.path.abspath(__file__))
docs = os.path.dirname(here)

# manual picture <- the test's picture
PARTS = {
    "sheet_leitner.png": "leitner_front_left.png",
    "sheet_xenakis.png": "leitner_xenakis_right.png",
    "strips.png": "leitner_default_strips.png",
    "room_front.png": "leitner_front_room.png",
    "room_turned.png": "leitner_turned_room.png",
    "space_controls.png": "leitner_front_controls.png",
    "room_paraboloid.png": "leitner_paraboloid_room.png",
    "room_polytope.png": "leitner_polytope_room.png",
    "sheet_drawing.png": "leitner_drawn_left.png",
    "room_drawing.png": "leitner_drawn_room.png",
}
# whole windows, as JPEGs in docs/
WHOLE = {
    "screenshot.jpg": "leitner_front.png",
    "front.jpg": "leitner_front.png",
    "turned.jpg": "leitner_turned.png",
    "paraboloid.jpg": "leitner_paraboloid.png",
    "polytope.jpg": "leitner_polytope.png",
}

if len(sys.argv) > 1:
    shots = os.path.abspath(sys.argv[1])
    for dst, src in PARTS.items():
        shutil.copyfile(os.path.join(shots, src), os.path.join(here, dst))
    for dst, src in WHOLE.items():
        subprocess.run(["sips", "-s", "format", "jpeg", "-s", "formatOptions", "88", os.path.join(shots, src),
                        "--out", os.path.join(docs, dst)], check=True, stdout=subprocess.DEVNULL)

chrome = next((c for c in ("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome", shutil.which("google-chrome"),
                           shutil.which("chromium")) if c and os.path.exists(c)), None)
if chrome is None:
    sys.exit("Chrome not found: open manual.html and print it to PDF by hand")
pdf = os.path.join(here, "Leitner Manual.pdf")
profile = tempfile.mkdtemp(prefix="leitner-manual-")   # its own profile, so a running Chrome is left alone
subprocess.run([chrome, "--headless", "--disable-gpu", "--user-data-dir=" + profile, "--no-pdf-header-footer", "--allow-file-access-from-files",
                "--print-to-pdf=" + pdf, "file://" + os.path.join(here, "manual.html")],
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
shutil.rmtree(profile, ignore_errors=True)
print("wrote", pdf)
