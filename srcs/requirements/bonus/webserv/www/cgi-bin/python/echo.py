#!/usr/bin/env python3
import os
import sys
import time
import urllib.parse
from datetime import datetime, timezone


def dump_env(prefix):
    for key in sorted(os.environ):
        if key.startswith(prefix):
            print(f"{key}={os.environ[key]}")


def read_relative_asset():
    asset_path = os.path.join(os.path.dirname(__file__), "..", "assets", "data.txt")
    try:
        with open(asset_path, "r", encoding="utf-8") as handle:
            return handle.read().strip()
    except OSError as exc:
        return f"(unable to read asset: {exc})"


body = sys.stdin.read()
query = os.environ.get("QUERY_STRING", "")
params = urllib.parse.parse_qs(query)
sleep_param = params.get("sleep", ["0"])[0]
try:
    sleep_for = min(float(sleep_param), 30.0)
except ValueError:
    sleep_for = 0.0
if sleep_for > 0:
    time.sleep(sleep_for)

sys.stdout.write("Status: 200 OK\r\n")
sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
sys.stdout.write("\r\n")
now = datetime.now(timezone.utc)
sys.stdout.write(f"Python CGI echo @ {now.isoformat()}\n")
print(f"REQUEST_METHOD={os.environ.get('REQUEST_METHOD', '')}")
print(f"SCRIPT_NAME={os.environ.get('SCRIPT_NAME', '')}")
print(f"PATH_INFO={os.environ.get('PATH_INFO', '')}")
print(f"QUERY_STRING={query}")
print(f"REMOTE_ADDR={os.environ.get('REMOTE_ADDR', '')}:{os.environ.get('REMOTE_PORT', '')}")
print(f"DOCUMENT_ROOT={os.environ.get('DOCUMENT_ROOT', '')}")
print()
sys.stdout.write("HTTP headers:\n")
dump_env("HTTP_")
sys.stdout.write("\n")
sys.stdout.write("Body:\n")
sys.stdout.write(body if body else "(empty)")
sys.stdout.write("\n\n")
sys.stdout.write("assets/data.txt -> " + read_relative_asset() + "\n")
