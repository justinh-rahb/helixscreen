#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Default values
VERSION=""
PLATFORM="all"
DRY_RUN=false
R2_BUCKET_NAME="${R2_BUCKET_NAME:-helixscreen-releases}"
CHANNEL="dev"

# Show usage
usage() {
    cat <<EOF
Usage: dev-release.sh [--version VERSION] [--platform PLATFORM] [--channel CHANNEL] [--dry-run] [--help]

Options:
  --version VERSION   Version string (default: VERSION.txt + -dev.TIMESTAMP)
  --platform PLATFORM Platform to upload (pi, ad5m, k1, or "all") (default: all available)
  --channel CHANNEL   Channel prefix in bucket (default: dev)
  --dry-run           Show what would be uploaded without actually uploading
  --help              Show this help message

Environment:
  R2_ACCOUNT_ID       Cloudflare account ID (required)
  R2_ACCESS_KEY_ID    R2 API token key (required)
  R2_SECRET_ACCESS_KEY R2 API token secret (required)
  R2_PUBLIC_URL       Public base URL, e.g. https://releases.helixscreen.org (required)
  R2_BUCKET_NAME      Bucket name (default: helixscreen-releases)
EOF
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --version)
            VERSION="$2"
            shift 2
            ;;
        --platform)
            PLATFORM="$2"
            shift 2
            ;;
        --channel)
            CHANNEL="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --help)
            usage
            ;;
        *)
            echo -e "${RED}Error: Unknown option $1${NC}" >&2
            usage
            ;;
    esac
done

# Check required tools
if ! command -v aws &> /dev/null; then
    echo -e "${RED}Error: aws CLI not found. Please install it first.${NC}" >&2
    exit 1
fi

if ! command -v jq &> /dev/null; then
    echo -e "${RED}Error: jq not found. Please install it first.${NC}" >&2
    exit 1
fi

# Check required environment variables
if [[ -z "${R2_ACCOUNT_ID:-}" ]]; then
    echo -e "${RED}Error: R2_ACCOUNT_ID environment variable not set${NC}" >&2
    exit 1
fi

if [[ -z "${R2_ACCESS_KEY_ID:-}" ]]; then
    echo -e "${RED}Error: R2_ACCESS_KEY_ID environment variable not set${NC}" >&2
    exit 1
fi

if [[ -z "${R2_SECRET_ACCESS_KEY:-}" ]]; then
    echo -e "${RED}Error: R2_SECRET_ACCESS_KEY environment variable not set${NC}" >&2
    exit 1
fi

if [[ -z "${R2_PUBLIC_URL:-}" ]]; then
    echo -e "${RED}Error: R2_PUBLIC_URL environment variable not set${NC}" >&2
    exit 1
fi

# Find git repo root
REPO_ROOT=$(git rev-parse --show-toplevel)
if [[ ! -f "$REPO_ROOT/VERSION.txt" ]]; then
    echo -e "${RED}Error: VERSION.txt not found in repo root${NC}" >&2
    exit 1
fi

# Determine version
if [[ -z "$VERSION" ]]; then
    BASE_VERSION=$(cat "$REPO_ROOT/VERSION.txt" | tr -d '\n')
    TIMESTAMP=$(date +%Y%m%d%H%M%S)
    VERSION="${BASE_VERSION}-dev.${TIMESTAMP}"
fi

echo -e "${GREEN}Version: $VERSION${NC}"

# Find build artifacts
BUILD_DIR="$REPO_ROOT/build/release"
if [[ ! -d "$BUILD_DIR" ]]; then
    echo -e "${RED}Error: Build directory not found: $BUILD_DIR${NC}" >&2
    exit 1
fi

# Determine which platforms to process
declare -a PLATFORMS=()
if [[ "$PLATFORM" == "all" ]]; then
    for tarball in "$BUILD_DIR"/helixscreen-*-*.tar.gz; do
        if [[ -f "$tarball" ]]; then
            filename=$(basename "$tarball")
            # Extract platform from filename: helixscreen-{platform}-*.tar.gz
            plat=$(echo "$filename" | sed -E 's/helixscreen-([^-]+)-.*/\1/')
            PLATFORMS+=("$plat")
        fi
    done

    if [[ ${#PLATFORMS[@]} -eq 0 ]]; then
        echo -e "${RED}Error: No tarballs found in $BUILD_DIR${NC}" >&2
        exit 1
    fi

    echo -e "${GREEN}Found platforms: ${PLATFORMS[*]}${NC}"
else
    # Validate platform
    if [[ ! "$PLATFORM" =~ ^(pi|ad5m|k1)$ ]]; then
        echo -e "${RED}Error: Invalid platform '$PLATFORM'. Must be pi, ad5m, k1, or all${NC}" >&2
        exit 1
    fi
    PLATFORMS=("$PLATFORM")
fi

# Determine sha256 command
if command -v shasum &> /dev/null; then
    SHA256_CMD="shasum -a 256"
elif command -v sha256sum &> /dev/null; then
    SHA256_CMD="sha256sum"
else
    echo -e "${RED}Error: Neither shasum nor sha256sum found${NC}" >&2
    exit 1
fi

# Build assets map
declare -A ASSETS
for plat in "${PLATFORMS[@]}"; do
    # Find the tarball for this platform
    tarball_pattern="$BUILD_DIR/helixscreen-${plat}-*.tar.gz"
    found=false

    for tarball in $tarball_pattern; do
        if [[ -f "$tarball" ]]; then
            found=true
            filename=$(basename "$tarball")

            # Compute SHA256
            echo -e "${YELLOW}Computing SHA256 for $filename...${NC}"
            sha256=$($SHA256_CMD "$tarball" | awk '{print $1}')

            # New filename with version
            new_filename="helixscreen-${plat}-v${VERSION}.tar.gz"

            # Store asset info
            ASSETS["${plat}_file"]="$tarball"
            ASSETS["${plat}_new_name"]="$new_filename"
            ASSETS["${plat}_sha256"]="$sha256"
            ASSETS["${plat}_url"]="${R2_PUBLIC_URL}/${CHANNEL}/${new_filename}"

            echo -e "${GREEN}  SHA256: $sha256${NC}"
            break
        fi
    done

    if [[ "$found" == "false" ]]; then
        echo -e "${RED}Error: No tarball found for platform '$plat' in $BUILD_DIR${NC}" >&2
        exit 1
    fi
done

# Generate manifest.json
GIT_SHORT_SHA=$(git rev-parse --short HEAD)
PUBLISHED_AT=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
TAG="v${VERSION}"

# Build JSON assets object
ASSETS_JSON="{"
first=true
for plat in "${PLATFORMS[@]}"; do
    if [[ "$first" == "false" ]]; then
        ASSETS_JSON+=","
    fi
    first=false

    ASSETS_JSON+="\"${plat}\":{"
    ASSETS_JSON+="\"url\":\"${ASSETS[${plat}_url]}\","
    ASSETS_JSON+="\"sha256\":\"${ASSETS[${plat}_sha256]}\""
    ASSETS_JSON+="}"
done
ASSETS_JSON+="}"

# Create full manifest
MANIFEST=$(jq -n \
    --arg version "$VERSION" \
    --arg tag "$TAG" \
    --arg notes "Dev build from $GIT_SHORT_SHA" \
    --arg published_at "$PUBLISHED_AT" \
    --argjson assets "$ASSETS_JSON" \
    '{
        version: $version,
        tag: $tag,
        notes: $notes,
        published_at: $published_at,
        assets: $assets
    }')

echo ""
echo -e "${GREEN}Generated manifest:${NC}"
echo "$MANIFEST" | jq .

# Set up AWS credentials for R2
export AWS_ACCESS_KEY_ID="$R2_ACCESS_KEY_ID"
export AWS_SECRET_ACCESS_KEY="$R2_SECRET_ACCESS_KEY"
R2_ENDPOINT="https://${R2_ACCOUNT_ID}.r2.cloudflarestorage.com"

if [[ "$DRY_RUN" == "true" ]]; then
    echo ""
    echo -e "${YELLOW}DRY RUN - Would execute the following commands:${NC}"
    echo ""

    for plat in "${PLATFORMS[@]}"; do
        source_file="${ASSETS[${plat}_file]}"
        dest_name="${ASSETS[${plat}_new_name]}"
        echo "aws s3 cp \"$source_file\" \"s3://${R2_BUCKET_NAME}/${CHANNEL}/${dest_name}\" --endpoint-url \"$R2_ENDPOINT\""
    done

    echo ""
    echo "# Upload manifest.json"
    echo "echo '$MANIFEST' | aws s3 cp - \"s3://${R2_BUCKET_NAME}/${CHANNEL}/manifest.json\" --endpoint-url \"$R2_ENDPOINT\" --content-type \"application/json\""

    echo ""
    echo -e "${GREEN}Manifest would be available at: ${R2_PUBLIC_URL}/${CHANNEL}/manifest.json${NC}"
else
    echo ""
    echo -e "${GREEN}Uploading tarballs...${NC}"

    for plat in "${PLATFORMS[@]}"; do
        source_file="${ASSETS[${plat}_file]}"
        dest_name="${ASSETS[${plat}_new_name]}"

        echo -e "${YELLOW}Uploading ${CHANNEL}/${dest_name}...${NC}"
        aws s3 cp "$source_file" "s3://${R2_BUCKET_NAME}/${CHANNEL}/${dest_name}" \
            --endpoint-url "$R2_ENDPOINT"
        echo -e "${GREEN}  Uploaded successfully${NC}"
    done

    echo ""
    echo -e "${GREEN}Uploading ${CHANNEL}/manifest.json...${NC}"
    echo "$MANIFEST" | aws s3 cp - "s3://${R2_BUCKET_NAME}/${CHANNEL}/manifest.json" \
        --endpoint-url "$R2_ENDPOINT" \
        --content-type "application/json"

    echo ""
    echo -e "${GREEN}✓ Upload complete!${NC}"
    echo -e "${GREEN}Manifest available at: ${R2_PUBLIC_URL}/${CHANNEL}/manifest.json${NC}"
fi
