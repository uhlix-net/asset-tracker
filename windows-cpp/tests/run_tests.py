"""Runs the C++ tests end to end on Windows.

    python run_tests.py <python-repo-root> <test-exe> <work-dir>

Needs a Python with the Python edition's requirements.txt installed. Runs on the
real Windows platform: Qt's offscreen platform has no fonts, so PDFs come out with
blocks for text. A few windows flash on screen.
"""
import os
import pathlib
import socket
import subprocess
import sys
import time

here = pathlib.Path(__file__).parent
repo, exe, work = sys.argv[1], pathlib.Path(sys.argv[2]), pathlib.Path(sys.argv[3])
fixtures, out = work / "fixtures", work / "out"
PORT = 18765

subprocess.run([sys.executable, here / "make_fixtures.py", repo, fixtures], check=True)

mock = subprocess.Popen([sys.executable, here / "mock_firebase.py", str(PORT)])
try:
    for _ in range(50):
        with socket.socket() as s:
            if s.connect_ex(("127.0.0.1", PORT)) == 0:
                break
        time.sleep(0.1)

    env = dict(os.environ,
               ASSET_TRACKER_FIXTURES=str(fixtures), ASSET_TRACKER_TEST_OUT=str(out),
               ASSET_TRACKER_MOCK=f"http://127.0.0.1:{PORT}")
    log = work / "test_log.txt"
    result = subprocess.run([exe, "-o", f"{log},txt"], env=env, cwd=exe.parent)
    print(log.read_text(encoding="utf-8", errors="replace"))
    if result.returncode != 0:
        sys.exit(f"C++ tests failed: {result.returncode}")

    subprocess.run([sys.executable, here / "verify_outputs.py", repo, fixtures, out, str(PORT)],
                   check=True)
finally:
    mock.terminate()
print("ALL PASSED")
