"""Fetch the Noto Sans SC WOFF2 subset used by the Simplified Chinese pages."""

from pathlib import Path
import re
from urllib.parse import urlencode
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parent.parent
pages = list((ROOT / "zh-hans").rglob("index.html"))
characters = sorted({
    char for page in pages for char in page.read_text(encoding="utf-8")
    if ord(char) >= 0x2E80
})
url = "https://fonts.googleapis.com/css2?" + urlencode({
    "family": "Noto Sans SC:wght@400..700",
    "text": "".join(characters),
})
request = Request(url, headers={
    "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                  "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36",
})
with urlopen(request, timeout=30) as response:
    css = response.read().decode("utf-8")
match = re.search(r"url\((https://fonts\.gstatic\.com/[^)]+)\) format\('woff2'\)", css)
if not match:
    raise SystemExit("Google Fonts did not return a WOFF2 subset")
with urlopen(match[1], timeout=30) as response:
    font = response.read()
if not font.startswith(b"wOF2"):
    raise SystemExit("The font response is not WOFF2")
(ROOT / "assets" / "noto-sans-sc-site.woff2").write_bytes(font)
