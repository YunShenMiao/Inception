#!/usr/bin/env python3
import os
import sys
import random


def get_random_cat_image():
    """Return a random cat image from the assets/cats directory"""
    asset_path = os.path.join(os.path.dirname(__file__), "..", "assets", "cats")
    
    try:
        if not os.path.isdir(asset_path):
            return None, None
        
        cat_files = [f for f in os.listdir(asset_path) 
                     if f.lower().endswith(('.jpg', '.jpeg', '.png', '.gif'))]
        
        if not cat_files:
            return None, None
        
        selected = random.choice(cat_files)
        file_path = os.path.join(asset_path, selected)
        
        # Determine MIME type
        ext = os.path.splitext(selected)[1].lower()
        mime_types = {
            '.jpg': 'image/jpeg',
            '.jpeg': 'image/jpeg',
            '.png': 'image/png',
            '.gif': 'image/gif'
        }
        mime = mime_types.get(ext, 'application/octet-stream')
        
        return file_path, mime
    except Exception as e:
        sys.stderr.write(f"Error: {e}\n")
        return None, None


def main():
    file_path, mime_type = get_random_cat_image()
    
    if not file_path or not os.path.isfile(file_path):
        sys.stdout.write("Status: 404 Not Found\r\n")
        sys.stdout.write("Content-Type: text/plain\r\n")
        sys.stdout.write("Content-Length: 14\r\n")
        sys.stdout.write("\r\n")
        sys.stdout.write("No cat image found")
        sys.stdout.flush()
        return
    
    try:
        file_size = os.path.getsize(file_path)
        
        sys.stdout.write("Status: 200 OK\r\n")
        sys.stdout.write(f"Content-Type: {mime_type}\r\n")
        sys.stdout.write(f"Content-Length: {file_size}\r\n")
        sys.stdout.write("\r\n")
        sys.stdout.flush()
        
        # Read and send binary data
        with open(file_path, 'rb') as f:
            sys.stdout.buffer.write(f.read())
        sys.stdout.buffer.flush()
    except Exception as e:
        sys.stderr.write(f"Error serving image: {e}\n")
        sys.stdout.write("Status: 500 Internal Server Error\r\n")
        sys.stdout.write("Content-Type: text/plain\r\n")
        sys.stdout.write("Content-Length: 21\r\n")
        sys.stdout.write("\r\n")
        sys.stdout.write("Internal server error")
        sys.stdout.flush()


if __name__ == "__main__":
    main()
