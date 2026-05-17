#!/usr/bin/env python3
"""
M5Clawd pairing helper.

Turns a Claude token into a QR code the M5Clawd device can scan during WiFi
setup, so the device never has to run its own OAuth login. Runs on macOS /
Windows / Linux — Python 3, standard library plus the `qrcode` package (this
script installs it on first run if it is missing).

How it fits together:
  1. You generate a long-lived Claude token with `claude setup-token`
     (Anthropic's official command — needs a Claude subscription). This
     helper can run it for you.
  2. The helper renders that token as a QR code, served at
     http://localhost:8765 so you can see it big on your computer screen.
  3. During M5Clawd's setup you join the device's WiFi, enter your home
     WiFi, then scan this QR with your phone's camera. The QR opens a URL on
     the device that hands it the token. The device then runs standalone.

Usage:
    python3 m5clawd-pair.py
"""

import http.server
import io
import socketserver
import subprocess
import sys
import urllib.parse
import webbrowser

DEVICE_AP_IP = "192.168.4.1"   # M5Clawd's fixed soft-AP address during setup
CRED_PATH = "/cred"            # device endpoint that ingests the token
LOCAL_PORT = 8765              # where this helper serves the QR page


def ensure_qrcode():
    """Import the `qrcode` library, installing it once if it is missing."""
    try:
        import qrcode
        import qrcode.image.svg  # noqa: F401
    except ImportError:
        print("Installing the 'qrcode' library (one-time)...")
        subprocess.check_call(
            [sys.executable, "-m", "pip", "install", "--quiet", "qrcode"])
        import qrcode
        import qrcode.image.svg  # noqa: F401
    return qrcode


def get_token():
    """Walk the user through obtaining a long-lived Claude token."""
    print()
    print("M5Clawd needs a long-lived Claude token. Anthropic's official")
    print("command for this is:  claude setup-token")
    print()
    answer = input("Run 'claude setup-token' now? [Y/n] ").strip().lower()
    if answer in ("", "y", "yes"):
        print("-" * 64)
        try:
            subprocess.run(["claude", "setup-token"])
        except FileNotFoundError:
            print("Could not find the 'claude' command. Install Claude Code,")
            print("or run 'claude setup-token' on another machine, then paste")
            print("the token below.")
        print("-" * 64)

    token = ""
    while not token:
        token = input("Paste the token, then press Enter:\n> ").strip()
        if token and not token.startswith("sk-ant-"):
            keep = input("That doesn't look like a Claude token (no "
                          "'sk-ant-' prefix). Use it anyway? [y/N] ")
            if keep.strip().lower() not in ("y", "yes"):
                token = ""
    return token


def build_qr_svg(qrcode, data):
    """Render `data` as an inline SVG QR-code string."""
    qr = qrcode.QRCode(
        border=2, error_correction=qrcode.constants.ERROR_CORRECT_L)
    qr.add_data(data)
    qr.make(fit=True)
    img = qr.make_image(image_factory=qrcode.image.svg.SvgPathImage)
    buf = io.BytesIO()
    img.save(buf)
    svg = buf.getvalue().decode("utf-8")
    # Drop the XML prolog so the SVG embeds cleanly inside HTML.
    return svg[svg.find("<svg"):]


def page_html(qr_svg, token):
    """The localhost QR page, styled to match the device."""
    return f"""<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>M5Clawd Pairing</title>
<style>
  :root {{ color-scheme: dark; }}
  body {{ margin:0; min-height:100vh; box-sizing:border-box;
         background:#1A1815; color:#F5F0E8; padding:40px 20px;
         font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;
         display:flex; flex-direction:column; align-items:center; }}
  h1 {{ color:#DA7756; font-size:1.6rem; margin:0 0 4px; }}
  .sub {{ color:#9b9590; margin:0 0 28px; text-align:center; }}
  .qr {{ background:#fff; padding:20px; border-radius:14px; width:300px;
        box-sizing:border-box; }}
  .qr svg {{ width:100%; height:auto; display:block; }}
  ol {{ max-width:430px; line-height:1.65; color:#d8d2c8; padding-left:22px; }}
  code {{ background:#2a2723; padding:2px 6px; border-radius:4px;
         color:#DA7756; font-size:.9em; }}
  details {{ max-width:430px; width:100%; color:#9b9590; }}
  summary {{ cursor:pointer; }}
  .token {{ word-break:break-all; background:#2a2723; padding:12px;
           border-radius:8px; font-size:.78rem; margin-top:8px; }}
</style></head><body>
  <h1>M5Clawd Pairing</h1>
  <p class="sub">Scan this with your phone during the device's WiFi setup.</p>
  <div class="qr">{qr_svg}</div>
  <ol>
    <li>On M5Clawd, finish the <b>Join WiFi</b> step and enter your home
        WiFi in the setup page.</li>
    <li>Still connected to the device's setup network, scan the QR above
        with your phone's <b>camera app</b>.</li>
    <li>It opens <code>http://192.168.4.1/cred</code> and hands the token to
        the device. M5Clawd restarts and begins showing your usage.</li>
  </ol>
  <details>
    <summary>Show token (manual fallback)</summary>
    <div class="token">{token}</div>
  </details>
</body></html>"""


def main():
    print("=" * 64)
    print(" M5Clawd pairing helper")
    print("=" * 64)
    qrcode = ensure_qrcode()
    token = get_token()

    cred_url = (f"http://{DEVICE_AP_IP}{CRED_PATH}"
                f"?t={urllib.parse.quote(token, safe='')}")
    html = page_html(build_qr_svg(qrcode, cred_url), token).encode("utf-8")

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(html)))
            self.end_headers()
            self.wfile.write(html)

        def log_message(self, *args):
            pass  # keep the console clean

    url = f"http://localhost:{LOCAL_PORT}"
    print()
    print(f"QR page ready:  {url}")
    print("Leave this running, scan the QR, then press Ctrl+C when done.")
    try:
        webbrowser.open(url)
    except Exception:
        pass

    with socketserver.TCPServer(("127.0.0.1", LOCAL_PORT), Handler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nDone.")


if __name__ == "__main__":
    main()
