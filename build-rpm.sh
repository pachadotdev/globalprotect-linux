#!/bin/bash
# Build RPM package for Fedora/RedHat using Docker

set -e

echo "Building RPM package for Fedora/RedHat..."

# Create output directory
mkdir -p dist

# Build Docker image
docker build -f Dockerfile.fedora -t globalprotect-rpm-builder .

# Run container and copy the built package
docker run --rm -v "$(pwd)/dist:/output" globalprotect-rpm-builder

echo "RPM package created successfully in dist/"
ls -lh dist/*.rpm
