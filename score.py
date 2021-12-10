#!/usr/bin/env python3


from pathlib import Path
import subprocess
import threading
from threading import Event
import time
import json
import signal
import os


script_dir = Path(__file__).parent


def read_from_pipe(proc, event, append_data):
    while not event.is_set():
        append_data(proc.stderr.read(1))


def test_submission(submission: Path, results):
    print(f"Testing {submission.name}")
    with subprocess.Popen(
        ["make", "run"], cwd=submission, stderr=subprocess.PIPE, universal_newlines=True, preexec_fn=os.setsid
    ) as proc:
        # Read from pipe in a thread
        event = Event()
        data = ""

        def append_data(new_data):
            print(new_data, end="")
            nonlocal data
            data += new_data

        thread = threading.Thread(target=read_from_pipe, args=(proc, event, append_data))
        thread.start()

        # Wait a while then terminate
        time.sleep(30)
        event.set()
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)

        results[submission.name] = data


def main():
    results = {}
    for submission in (script_dir / "submissions").glob("*"):
        if submission.is_dir() and all(child.name != "TBD" for child in submission.iterdir()):
            test_submission(submission, results)

    with open("results.json", "w") as f:
        json.dump(results, f)


if __name__ == "__main__":
    main()
