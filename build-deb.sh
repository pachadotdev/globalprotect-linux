#!/bin/bash
# Build DEB package for Ubuntu/Debian using Docker

set -e

echo "Building DEB package for Ubuntu/Debian..."

# Create output directory
mkdir -p dist

# Build Docker image
docker build -f Dockerfile.debian -t globalprotect-deb-builder .

# Run container and copy the built package
docker run --rm -v "$(pwd)/dist:/output" globalprotect-deb-builder

echo "DEB package created successfully in dist/"
ls -lh dist/*.deb
