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
autograder --bundle hw3.zip      # one zip/folder; files sorted by name
autograder --rubric rubric.pdf --solution my_work.zip [--files assignment.zip] --name hw3
```

| Flag | Meaning |
|---|---|
| `--rubric FILE` | Rubric as text, markdown, or PDF |
| `--bundle PATH` | A zip or folder with everything in it, sorted by file name (see below) |
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

## Watch mode (send a bundle, get feedback back)

Run `autograder --watch` on the Pi. On the Mac, put everything for one assignment (the rubric,
your work, and the spec or starter code if you want to include them) in **one zip or folder**
and Taildrop it to the Pi. In Finder: right-click it, then **Share**, then **Tailscale**.

Files are sorted by name. A top-level folder that wraps everything (as when you zip a
folder) doesn't count:

| Name | Treated as |
|---|---|
| contains `rubric` (e.g. `rubric.pdf`, `hw3-rubric.md`) | rubric |
| a file or folder named with `hw`, `hwk` or `homework` as a word (e.g. `hw3.py`, `HW_3/`, `homework3.zip`) | your homework; all files inside a matching folder count |
| everything else | context (spec, starter code, data) |

Then it grades with `gpt-6.1-sol` and Taildrops `<name>-feedback.zip` back to `mbp`. If something
fails, for example there's no rubric, it sends an `ERROR-....md` explaining why.

A bundle is processed once nothing new has arrived for 15 seconds (`--settle`). Each zip or
folder is its own bundle, and loose files sent together count as one bundle. Received files
are kept in `~/.autograder/jobs/<name>-<time>/` and results go to `~/.autograder/out/`.

You can do the same thing without watch mode: `autograder --bundle hw3.zip`.
Add `--dry-run` to see how the files were sorted and the prompt that would be sent.
To receive Taildrop files without `sudo`, run this once: `sudo tailscale set --operator=$USER`.

### Run it in the background on boot (systemd)

```sh
mkdir -p ~/.config/systemd/user
cat > ~/.config/systemd/user/autograder.service <<'UNIT'
[Unit]
Description=autograder Taildrop watcher
After=network-online.target

[Service]
ExecStart=/usr/local/bin/autograder --watch
Restart=on-failure
RestartSec=30

[Install]
WantedBy=default.target
UNIT
systemctl --user daemon-reload
systemctl --user enable --now autograder
sudo loginctl enable-linger $USER    # keep it running when you're logged out
journalctl --user -u autograder -f    # follow the log
```

Exit codes: `0` ok, `1` input/other error, `2` API error, `3` Taildrop failed (results still saved locally).
The raw API reply is saved as `response.json` in the output folder for debugging.
