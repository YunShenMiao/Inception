#!/usr/bin/env python3
"""CGI script with infinite loop for testing timeout handling."""

import sys

print("Content-Type: text/plain")
print("Status: 200 OK")
print()
print("Starting infinite loop...")
sys.stdout.flush()

# Infinite loop - server should timeout and kill this
counter = 0
while True:
    counter += 1
