# License
[![CC BY-SA 4.0][cc-by-sa-shield]][cc-by-sa]

This work is licensed under a
[Creative Commons Attribution-ShareAlike 4.0 International License][cc-by-sa].

[![CC BY-SA 4.0][cc-by-sa-image]][cc-by-sa]

[cc-by-sa]: http://creativecommons.org/licenses/by-sa/4.0/
[cc-by-sa-image]: https://licensebuttons.net/l/by-sa/4.0/88x31.png
[cc-by-sa-shield]: https://img.shields.io/badge/License-CC%20BY--SA%204.0-lightgrey.svg

All submissions are licensed under the terms of CC BY-SA 4.0 (see https://codegolf.stackexchange.com/help/licensing) and their authors are named in their respective directories (name approximately copied from the code golf stack exchange username who made the submission).

Slight modifications may have been made to the submissions which include formatting / small adjustments that were needed to build/run them.

# Overview
This repo contains the submissions to the https://codegolf.stackexchange.com/questions/215216/high-throughput-fizz-buzz code golf question and some code to build/run and plot their results.

# Files
`score.py` - Runs all submissions and stores the `pv` output of all the submissions in a single JSON file called `results.json`

`plot.py` - Run this after running `score.py` to generate a graph from `results.json`

`submissions` - A directory containing the actual code of all submissions, each with a Makefile that has a `run` target that knows how to build/run the submission

# TODO
- [ ] Fix all TBD submissions (Powershell, Windows-only, C#)
- [ ] Automate installation of all the toolchains
- [ ] Generate a table with all the results and not just the plot
- [ ] Choose one submission as reference and compare the output of
      all other submissions to that to make sure their output is actually valid. 
      May require small adjustments to capitalization of Fizz and Buzz
- [ ] Separate graph per programming language - a graph with all of them is too crowded

# Requirements
A fairly modern Java version, clang, g++, gcc, Ruby, Rust, Julia, Python3, pypy3, and maybe some other toolchains I forgot

# Contributing
If you want to change the content of your submission / how it's ran/built or improve anything else feel free to create pull requests

