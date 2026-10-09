# autograder

A command-line tool that grades your submission against a rubric using the OpenAI API, then:

- writes **TODO.md**: an in-depth, bulleted checklist of changes, ordered by points at stake
- writes **STUDY.md**: topics to study, each tied to what went wrong, with how to practice it
- updates **RECURRING.md** (global, `~/.autograder/RECURRING.md`): a table of the kinds of mistakes you make, where **Weight** is the number of assignments each one has appeared in. The model sees the existing list so it can match repeats to existing entries.
- writes **SUMMARY.md**: estimated score, overall assessment, and which errors were repeats
- zips those four files and Taildrops the zip to `mbp`

## Build (Raspberry Pi OS / Debian)

```sh
sudo apt install build-essential cmake libcurl4-openssl-dev nlohmann-json3-dev zip unzip
cmake -B build -S . && cmake --build build -j
ctest --test-dir build          # optional
sudo cmake --install build      # installs to /usr/local/bin/autograder
```

On macOS: `brew install cmake` (if nlohmann-json isn't installed, CMake downloads it).
Taildrop requires `tailscale` to be installed and logged in.

## Setup

Put your key in `~/.autograder/config` (and `chmod 600` it), or export `OPENAI_API_KEY`:

```
api_key=sk-...
# optional defaults:
# model=gpt-6.1-sol
# target=mbp
# recurring=~/.autograder/RECURRING.md
# out=~/autograder-out
# max_wait=900
```

## Usage

```sh
autograder --rubric rubric.pdf --files assignment.zip --solution my_work.zip --name hw3
```

| Flag | Meaning |
|---|---|
| `--rubric FILE` | Rubric as text, markdown, or PDF |
| `--files ZIP` | Optional context: assignment spec, starter code, data |
| `--solution PATH` | Your submission, as a zip or a single file |
| `--name NAME` | Assignment label (default: solution file name) |
| `--model MODEL` | Default `gpt-6.1-sol` |
| `--out DIR` | Default `./autograder-out`; each run gets `<name>-<timestamp>/` |
| `--target DEVICE` | Taildrop target, default `mbp` |
| `--no-send` | Skip Taildrop |
| `--dry-run` | Print the prompt that would be sent; no API call |
| `--max-wait SECONDS` | Total time to spend waiting out rate limits (default 900) |
| `--max-chars N` | Character budget for file text sent (default 300000, ~75k tokens) |

PDFs are attached as files. Other text files are inlined. Binaries, `__MACOSX`, `.git`, etc. are skipped. Anything skipped or truncated is listed in SUMMARY.md.

**Rate limits:** each run makes a single API request. On 429 or 5xx responses it waits as long as
OpenAI's `retry-after` / `x-ratelimit-reset-*` headers say, or otherwise backs off exponentially
up to 2 minutes per wait, until `--max-wait` runs out. A request too large for your tokens-per-minute
limit fails right away, because retrying can't help; lower `--max-chars` if that happens.

Exit codes: `0` ok, `1` input/other error, `2` API error, `3` Taildrop failed (results still saved locally).
The raw API reply is saved as `response.json` in the output folder for debugging.
