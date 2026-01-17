#!/bin/bash
# Build DEB package for Ubuntu/Debian using Docker

set -e

echo "Building DEB package for Ubuntu/Debian..."

# Create output directory
mkdir -p dist

# Build Docker image
docker build -f Dockerfile.debian -t globalprotect-deb-builder .

# Create a temporary container from the image (do not start it)
container_id=$(docker create globalprotect-deb-builder)

# Copy the built .deb from the image filesystem to the host `dist/` directory
docker cp "$container_id:/output/globalprotect-openconnect_0.0.1_amd64.deb" "$(pwd)/dist/"

# Remove the temporary container
docker rm "$container_id" > /dev/null

echo "DEB package created successfully in dist/"
ls -lh dist/*.deb
