#!/usr/bin/env python3
# Helper script to bypass the vendor 'imi' binary MD5 verification.
# It calculates the MD5 of the file up to the first NULL byte and appends it to the end.

import sys
import hashlib

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <file>")
        sys.exit(1)

    filepath = sys.argv[1]
    
    with open(filepath, "rb") as f:
        data = f.read()

    # The vendor 'imi' uses strlen() to determine the data length for MD5.
    # We find the first NULL byte (0x00).
    idx = data.find(b"\x00")
    
    if idx == -1:
        # If no NULL byte found, hash the whole thing (unlikely for a tarball)
        hash_data = data
    else:
        hash_data = data[:idx]

    md5_hash = hashlib.md5(hash_data).digest()

    # Append the 16-byte MD5 to the end of the file
    with open(filepath, "ab") as f:
        f.write(md5_hash)

if __name__ == "__main__":
    main()
