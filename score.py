#!/usr/bin/env python3


from pathlib import Path
import subprocess
import threading
from threading import Event
import time
import json
import signal
import os
import sys
from collections import defaultdict


script_dir = Path(__file__).parent


def read_from_pipe(proc, event, append_data):
    while not event.is_set():
        append_data(proc.stderr.read(1))


def test_submission(submission: Path):
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
        time.sleep(60)
        event.set()
        os.killpg(os.getpgid(proc.pid), signal.SIGTERM)

        return data


def main():
    if len(sys.argv) == 3:
        # Run just the language/submission combo specified in the command line
        force = (sys.argv[1], sys.argv[2])
    else:
        # Run all languages/submissions
        force = None

    results_file = script_dir / "results.json"

    results = defaultdict(dict)
    if force is not None:
        # Try to load all results from previous results file
        if results_file.exists():
            with open(results_file, "r") as f:
                results = json.load(f)

    for lang in (script_dir / "submissions").glob("*"):
        for submission in lang.glob("*"):
            if submission.is_dir() and not any(child.name == "TBD" or child.name == "DISQUALIFIED" for child in submission.iterdir()):
                if force is not None:
                    force_language, force_submission = force
                    if lang.name != force_language or submission.name != force_submission:
                        print(f"Skipping {lang.name} {submission.name}")
                        continue

                print(f"Testing {lang.name} {submission.name}")
                results[lang.name][submission.name] = test_submission(submission)

    with open(results_file, "w") as f:
        json.dump(results, f)


if __name__ == "__main__":
    main()
