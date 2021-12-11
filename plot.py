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
    pat = re.compile(r"[0-9.a-zA-Z\s]+ [0-9:]+ \[\s*([0-9.]+)\s*([a-zA-Z]+)/s\].*")
    results = pat.findall(line)

    if results:
        value, unit = results[0]
        return float(value) / (1 if unit == "GiB" else 1024)

    print(f"Couldn't parse line: {line} with {pat} {results}")
    return 0.0


def main():
    with open(script_dir / "results.json", "r") as f:
        results = json.load(f)

    output_dir = script_dir / "results"
    output_dir.mkdir(exist_ok=True)

    for language, submissions in results.items():
        parsed_results = {
            k: [
                parse_line(line) for line in v.splitlines()
            ] for k, v in submissions.items()
        }

        for submission, values in parsed_results.items():
            p = plt.plot(values, ".", label=submission)
            mean = np.mean(values)
            plt.axhline(mean, color=p[0].get_color())

        plt.legend()
        plt.xlabel("Seconds")
        plt.ylabel("GiB/s")
        plt.title(language)
        plt.gcf().set_size_inches(12, 8)
        plt.savefig(output_dir / f"{language}.png")
        plt.clf()


if __name__ == "__main__":
    main()
