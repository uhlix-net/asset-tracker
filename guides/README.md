# Guides

User-facing documents, written for people who are not comfortable with computers.

| Document | Latest version |
|---|---|
| Online sync setup (Firebase) | `Asset-Tracker-Online-Sync-Setup-2026.001.pdf` |

## Versions

Versions are `yyyy.nnn`: the year, then a release number that restarts at `001` each
year. `2026.001` is the first release of 2026. Released PDFs are never overwritten;
a change produces a new file with the next number, and older versions stay here.

## Regenerating

Each PDF is built from a script in `src/`. To make a new version, edit the text,
bump `VERSION` and `RELEASED`, then run:

```
pip install reportlab
python src/make_sync_setup_guide.py
```

The script needs the DejaVu Sans fonts (installed on most Linux systems). It refuses
to overwrite an existing version.
