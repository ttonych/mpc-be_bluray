"""Extract the verified release without relying on the system tar executable."""
import sys
import tarfile

with tarfile.open(sys.argv[1], "r:xz") as archive:
    archive.extractall(sys.argv[2], filter="data")
