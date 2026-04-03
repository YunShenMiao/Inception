#!/usr/bin/env python3
"""CGI script with intentional error for testing error handling."""

import sys

print("Content-Type: text/plain")
print("Status: 200 OK")
print()
print("About to crash...")
sys.stdout.flush()

# Intentional error - division by zero
result = 1 / 0
print(f"This will never print: {result}")
