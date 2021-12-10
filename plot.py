#!/usr/bin/env python3

from matplotlib import pyplot as plt
import numpy as np
import json
import re
from pathlib import Path

script_dir = Path(__file__).parent.absolute()


def parse_line(line):
    pat = re.compile(r"[0-9.a-zA-Z]+ [0-9:]+ \[([0-9.]+)([a-zA-Z]+)/s\].*")
    results = pat.findall(line)
    if results:
        return results[0]

    return (None, None)


def main():
    with open(script_dir / "results.json", "r") as f:
        results = json.load(f)

    parsed_results = {k: [parse_line(line) for line in v.splitlines()] for k, v in results.items()}

    for submission, values in parsed_results.items():
        values_float = [float(value) / (1 if unit == "GiB" else 1024) for value, unit in values if value]
        mean = np.mean(values_float)
        p = plt.plot(values_float, ".", label=submission)
        plt.axhline(mean, color=p[0].get_color())

    plt.legend()
    plt.xlabel("Seconds")
    plt.ylabel("GiB/s")
    plt.show()


if __name__ == "__main__":
    main()
