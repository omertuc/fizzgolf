#!/usr/bin/env python3

from matplotlib import pyplot as plt
import numpy as np
import json
import re
from pathlib import Path

script_dir = Path(__file__).parent.absolute()


def parse_line(line):
    # Example line:
    # 833MiB 0:00:01 [ 833MiB/s] [<=>                                               ]
    # Extracts ("833", "MiB")
    pat = re.compile(r"[0-9.a-zA-Z]+ [0-9:]+ \[([0-9.]+)([a-zA-Z]+)/s\].*")
    results = pat.findall(line)

    if results:
        value, unit = results[0]
        return float(value) / (1 if unit == "GiB" else 1024)

    return 0.0


def main():
    with open(script_dir / "results.json", "r") as f:
        results = json.load(f)

    def remove_none(x):
        return x[1]

    parsed_results = {
        k: [
            parse_line(line) for line in v.splitlines()
        ] for k, v in results.items()
    }

    for submission, values in parsed_results.items():
        p = plt.plot(values, ".", label=submission)
        mean = np.mean(values)
        plt.axhline(mean, color=p[0].get_color())

    plt.legend()
    plt.xlabel("Seconds")
    plt.ylabel("GiB/s")
    plt.show()


if __name__ == "__main__":
    main()
