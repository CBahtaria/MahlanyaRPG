#!/usr/bin/env bash
# =============================================================================
# Mahlanya RPG — EC2 Spot Build Agent Bootstrap (user-data)
# =============================================================================
# Runs as root on first boot of every c5.4xlarge Spot instance launched by the
# MahlanyaBuildStack Spot fleet.
#
# Steps:
#   1. System update + UE5 prerequisites (clang, cmake, mono, dotnet)
#   2. Zig 0.14.0 (for libmahlanya_compute.so cross-compilation)
#   3. GDAL + Python 3.11 pipeline dependencies
#   4. Mount EFS volume to /mnt/mahlanya-content
#   5. Download libmahlanya_compute.so from S3 if not already cached on EFS
#   6. Persist MAHLANYA_ZIG_LIB_PATH in /etc/environment
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Logging helper — all output goes to /var/log/mahlanya-userdata.log as well
# as the default cloud-init log (journald / /var/log/cloud-init-output.log).
# ---------------------------------------------------------------------------
LOG=/var/log/mahlanya-userdata.log
exec > >(tee -a "$LOG") 2>&1

log() {
    echo "[$(date -u +%Y-%m-%dT%H:%M:%SZ)] $*"
}

log "=== Mahlanya build agent bootstrap starting ==="

# ---------------------------------------------------------------------------
# Region / instance metadata (IMDSv2 required by the launch template)
# ---------------------------------------------------------------------------
TOKEN=$(curl -sf -X PUT \
    "http://169.254.169.254/latest/api/token" \
    -H "X-aws-ec2-metadata-token-ttl-seconds: 21600")

AWS_REGION=$(curl -sf \
    "http://169.254.169.254/latest/meta-data/placement/region" \
    -H "X-aws-ec2-metadata-token: $TOKEN")

INSTANCE_ID=$(curl -sf \
    "http://169.254.169.254/latest/meta-data/instance-id" \
    -H "X-aws-ec2-metadata-token: $TOKEN")

log "Region: $AWS_REGION  |  Instance: $INSTANCE_ID"

# EFS file-system ID is passed as an EC2 tag set by CDK at fleet launch time.
EFS_ID=$(aws ec2 describe-tags \
    --region "$AWS_REGION" \
    --filters "Name=resource-id,Values=$INSTANCE_ID" \
              "Name=key,Values=MahlanyaEFSId" \
    --query 'Tags[0].Value' \
    --output text)

S3_BUCKET="mahlanya-pipeline-artifacts"
LIB_REMOTE_PATH="s3://${S3_BUCKET}/libmahlanya_compute.so"
EFS_MOUNT="/mnt/mahlanya-content"
LIB_LOCAL_PATH="${EFS_MOUNT}/lib/libmahlanya_compute.so"
ZIG_VERSION="0.14.0"
ZIG_INSTALL_DIR="/opt/zig"
PYTHON_VERSION="3.11"

# ===========================================================================
# STEP 1 — System update + UE5 Linux prerequisites
# ===========================================================================
log "Step 1: Installing UE5 prerequisites..."

dnf update -y --quiet

# Core build tools
dnf install -y --quiet \
    clang \
    clang-tools-extra \
    cmake \
    ninja-build \
    git \
    curl \
    tar \
    unzip \
    file \
    lsb-release \
    openssl-devel \
    libstdc++-devel \
    libstdc++-static \
    ncurses-devel \
    libuuid-devel \
    patch \
    patchutils \
    dos2unix

# Mono (required by UE5 C# build tools — UnrealBuildTool)
# Amazon Linux 2023 ships mono in its package repo
dnf install -y --quiet mono-devel || {
    log "mono not in default repo — installing from mono-project.com..."
    rpm --import "https://keyserver.ubuntu.com/pks/lookup?op=get&search=0xA6A19B38D3D831EF"
    dnf config-manager --add-repo \
        "https://download.mono-project.com/repo/centos8-stable.repo" 2>/dev/null || true
    dnf install -y --quiet mono-devel
}

# .NET 8 SDK (UE5 also calls dotnet for some tooling)
dnf install -y --quiet dotnet-sdk-8.0 || {
    log "dotnet-sdk-8.0 not in default repo — using Microsoft package feed..."
    rpm --import "https://packages.microsoft.com/keys/microsoft.asc"
    cat > /etc/yum.repos.d/microsoft-prod.repo <<'EOF'
[packages-microsoft-com-prod]
name=packages-microsoft-com-prod
baseurl=https://packages.microsoft.com/rhel/8/prod/
enabled=1
gpgcheck=1
gpgkey=https://packages.microsoft.com/keys/microsoft.asc
EOF
    dnf install -y --quiet dotnet-sdk-8.0
}

log "Step 1 complete."

# ===========================================================================
# STEP 2 — Zig 0.14.0
# ===========================================================================
log "Step 2: Installing Zig ${ZIG_VERSION}..."

ZIG_TARBALL="zig-linux-x86_64-${ZIG_VERSION}.tar.xz"
ZIG_URL="https://ziglang.org/download/${ZIG_VERSION}/${ZIG_TARBALL}"

curl -fsSL "$ZIG_URL" -o "/tmp/${ZIG_TARBALL}"
mkdir -p "$ZIG_INSTALL_DIR"
tar -xJf "/tmp/${ZIG_TARBALL}" -C "$ZIG_INSTALL_DIR" --strip-components=1
rm "/tmp/${ZIG_TARBALL}"

# Make zig available system-wide
ln -sf "${ZIG_INSTALL_DIR}/zig" /usr/local/bin/zig

zig version
log "Step 2 complete: $(zig version)"

# ===========================================================================
# STEP 3 — GDAL + Python 3.11 pipeline dependencies
# ===========================================================================
log "Step 3: Installing GDAL and Python ${PYTHON_VERSION} pipeline deps..."

# GDAL headers and runtime (for rasterio / geopandas build wheels)
dnf install -y --quiet \
    gdal \
    gdal-devel \
    gdal-libs \
    proj \
    proj-devel \
    geos \
    geos-devel

# Python 3.11 (AL2023 ships 3.11 in base repo)
dnf install -y --quiet \
    "python${PYTHON_VERSION}" \
    "python${PYTHON_VERSION}-devel" \
    "python${PYTHON_VERSION}-pip"

# Upgrade pip, then install the full pipeline requirements
PYTHON="python${PYTHON_VERSION}"
$PYTHON -m pip install --quiet --upgrade pip setuptools wheel

# Install pipeline deps matching pipeline/requirements.txt
$PYTHON -m pip install --quiet \
    "gdal==3.8.*" \
    "rasterio>=1.3.0" \
    "shapely>=2.0.0" \
    "geopandas>=0.14.0" \
    "pyproj>=3.6.0" \
    "numpy>=1.26.0" \
    "scipy>=1.12.0" \
    "numba>=0.59.0" \
    "trimesh>=4.0.0" \
    "neo4j>=5.0.0" \
    "click>=8.1.0" \
    "pyyaml>=6.0.0"

log "Step 3 complete."

# ===========================================================================
# STEP 4 — Mount EFS volume to /mnt/mahlanya-content
# ===========================================================================
log "Step 4: Mounting EFS ${EFS_ID} -> ${EFS_MOUNT}..."

# amazon-efs-utils gives us the `efs` mount helper (TLS, IAM auth)
dnf install -y --quiet amazon-efs-utils

mkdir -p "$EFS_MOUNT"

# Add to /etc/fstab for remount on reboot
FSTAB_ENTRY="${EFS_ID}:/ ${EFS_MOUNT} efs _netdev,tls,iam 0 0"
if ! grep -qF "$EFS_ID" /etc/fstab; then
    echo "$FSTAB_ENTRY" >> /etc/fstab
    log "Added EFS entry to /etc/fstab"
fi

# Mount now (retries handle transient EFS DNS propagation delays)
MOUNT_RETRIES=5
for i in $(seq 1 $MOUNT_RETRIES); do
    if mount -t efs -o tls,iam "${EFS_ID}:/" "$EFS_MOUNT"; then
        log "EFS mounted successfully on attempt ${i}."
        break
    fi
    if [ "$i" -eq "$MOUNT_RETRIES" ]; then
        log "ERROR: EFS mount failed after ${MOUNT_RETRIES} attempts — aborting."
        exit 1
    fi
    log "EFS mount attempt ${i} failed; retrying in 10 s..."
    sleep 10
done

# Ensure the lib sub-directory exists on the shared volume
mkdir -p "${EFS_MOUNT}/lib"

log "Step 4 complete."

# ===========================================================================
# STEP 5 — Download libmahlanya_compute.so from S3 if not already cached
# ===========================================================================
log "Step 5: Checking for libmahlanya_compute.so on EFS..."

if [ -f "$LIB_LOCAL_PATH" ]; then
    log "libmahlanya_compute.so already present at ${LIB_LOCAL_PATH} — skipping download."
else
    log "Downloading ${LIB_REMOTE_PATH} -> ${LIB_LOCAL_PATH}..."
    aws s3 cp \
        --region "$AWS_REGION" \
        "$LIB_REMOTE_PATH" \
        "$LIB_LOCAL_PATH"
    chmod 755 "$LIB_LOCAL_PATH"
    log "Download complete."
fi

log "Step 5 complete."

# ===========================================================================
# STEP 6 — Persist MAHLANYA_ZIG_LIB_PATH in /etc/environment
# ===========================================================================
log "Step 6: Setting MAHLANYA_ZIG_LIB_PATH in /etc/environment..."

ENV_FILE="/etc/environment"
ENV_VAR="MAHLANYA_ZIG_LIB_PATH=${EFS_MOUNT}/lib"

if grep -q "MAHLANYA_ZIG_LIB_PATH" "$ENV_FILE" 2>/dev/null; then
    # Update existing value in-place (handles re-runs of user-data)
    sed -i "s|^MAHLANYA_ZIG_LIB_PATH=.*|${ENV_VAR}|" "$ENV_FILE"
    log "Updated existing MAHLANYA_ZIG_LIB_PATH."
else
    echo "$ENV_VAR" >> "$ENV_FILE"
    log "Appended MAHLANYA_ZIG_LIB_PATH to ${ENV_FILE}."
fi

# Also export for any processes launched later in this script
export MAHLANYA_ZIG_LIB_PATH="${EFS_MOUNT}/lib"

log "Step 6 complete."

# ===========================================================================
# Done
# ===========================================================================
log "=== Mahlanya build agent bootstrap complete ==="
log "    Zig:               $(zig version)"
log "    Python:            $($PYTHON --version)"
log "    EFS mount:         ${EFS_MOUNT}"
log "    libmahlanya path:  ${MAHLANYA_ZIG_LIB_PATH}"
