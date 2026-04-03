#!/usr/bin/env python3
import os
import sys
import urllib.parse


def format_params(query_string: str) -> str:
    params = urllib.parse.parse_qs(query_string, keep_blank_values=True)
    if not params:
        return "(no query parameters provided)"
    lines = []
    for key in sorted(params):
        values = ", ".join(params[key])
        lines.append(f"{key} = {values}")
    return "\n".join(lines)


query = os.environ.get("QUERY_STRING", "")

sys.stdout.write("Status: 200 OK\r\n")
sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("Python CGI query parser demo\n")
sys.stdout.write(f"Raw QUERY_STRING: {query or '(empty)'}\n\n")
sys.stdout.write("Decoded parameters:\n")
sys.stdout.write(format_params(query) + "\n")
