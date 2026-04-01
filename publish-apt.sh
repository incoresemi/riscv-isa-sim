#!/usr/bin/env bash
set -euo pipefail

# ============================================================================
#  publish-apt.sh — Publish .deb packages to a GitHub Pages apt repository
#
#  Usage:
#    ./publish-apt.sh <deb-file> [distros...]
#
#  Examples:
#    ./publish-apt.sh riscv-toolchain_2.0.3_amd64.deb                # defaults: jammy noble
#    ./publish-apt.sh riscv-toolchain_2.0.3_amd64.deb jammy noble focal
#
#  The script:
#    1. Checks out (or creates) the gh-pages branch in a temp worktree
#    2. Copies the .deb into pool/main/
#    3. Generates Packages, Release, InRelease for each distro
#    4. Exports the GPG public key
#    5. Commits and pushes to gh-pages
#    6. Cleans up the worktree
#
#  Users install with:
#    curl -fsSL https://makarkul.github.io/riscv-isa-sim/key.gpg | \
#        sudo gpg --dearmor -o /usr/share/keyrings/riscv-toolchain.gpg
#    echo "deb [signed-by=/usr/share/keyrings/riscv-toolchain.gpg] \
#        https://makarkul.github.io/riscv-isa-sim/ jammy main" | \
#        sudo tee /etc/apt/sources.list.d/riscv-toolchain.list
#    sudo apt update && sudo apt install riscv-toolchain
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GPG_KEY_ID="RISC-V Toolchain"
DEFAULT_DISTROS="jammy noble"
COMPONENT="main"
ARCH="amd64"

usage() {
    echo "Usage: $0 <deb-file> [distro1 distro2 ...]"
    echo ""
    echo "  Default distros: $DEFAULT_DISTROS"
    echo ""
    echo "Examples:"
    echo "  $0 riscv-toolchain_2.0.3_amd64.deb"
    echo "  $0 riscv-toolchain_2.0.3_amd64.deb jammy noble focal"
    exit 1
}

[ $# -lt 1 ] && usage

DEB_FILE="$(realpath "$1")"
shift

if [ ! -f "$DEB_FILE" ]; then
    echo "Error: File not found: $DEB_FILE"
    exit 1
fi

DISTROS="${*:-$DEFAULT_DISTROS}"
DEB_BASENAME="$(basename "$DEB_FILE")"
PKG_NAME="$(dpkg-deb --field "$DEB_FILE" Package)"
PKG_VERSION="$(dpkg-deb --field "$DEB_FILE" Version)"

echo "==> Publishing $DEB_BASENAME (v$PKG_VERSION) for: $DISTROS"

# --- Set up a temporary worktree for gh-pages ---
WORKTREE="$(mktemp -d)"
BRANCH="gh-pages"

# Check if gh-pages branch exists
if git show-ref --verify --quiet "refs/heads/$BRANCH" 2>/dev/null; then
    echo "==> Checking out existing $BRANCH branch..."
    git worktree add "$WORKTREE" "$BRANCH"
elif git show-ref --verify --quiet "refs/remotes/origin/$BRANCH" 2>/dev/null; then
    echo "==> Checking out $BRANCH from remote..."
    git worktree add "$WORKTREE" -b "$BRANCH" "origin/$BRANCH"
else
    echo "==> Creating new $BRANCH branch (orphan)..."
    git worktree add --detach "$WORKTREE"
    cd "$WORKTREE"
    git checkout --orphan "$BRANCH"
    git rm -rf . 2>/dev/null || true
    cd "$SCRIPT_DIR"
fi

cd "$WORKTREE"

# --- Copy .deb into pool ---
POOL_DIR="pool/$COMPONENT"
mkdir -p "$POOL_DIR"
cp "$DEB_FILE" "$POOL_DIR/"
echo "    Copied $DEB_BASENAME to $POOL_DIR/"

# --- Generate metadata for each distro ---
for DISTRO in $DISTROS; do
    echo "==> Generating metadata for $DISTRO..."

    DIST_DIR="dists/$DISTRO/$COMPONENT/binary-$ARCH"
    mkdir -p "$DIST_DIR"

    # Generate Packages file
    dpkg-scanpackages --arch "$ARCH" "$POOL_DIR" > "$DIST_DIR/Packages"
    gzip -9c "$DIST_DIR/Packages" > "$DIST_DIR/Packages.gz"

    PKG_COUNT=$(grep -c "^Package:" "$DIST_DIR/Packages" || echo 0)
    echo "    $DIST_DIR/Packages ($PKG_COUNT packages)"

    # Generate Release file
    cd "dists/$DISTRO"
    apt-ftparchive release \
        -o APT::FTPArchive::Release::Origin="makarkul" \
        -o APT::FTPArchive::Release::Label="RISC-V Toolchain" \
        -o APT::FTPArchive::Release::Suite="$DISTRO" \
        -o APT::FTPArchive::Release::Codename="$DISTRO" \
        -o APT::FTPArchive::Release::Architectures="$ARCH" \
        -o APT::FTPArchive::Release::Components="$COMPONENT" \
        . > Release

    # Sign: create both InRelease (inline) and Release.gpg (detached)
    gpg --yes --default-key "$GPG_KEY_ID" --clearsign -o InRelease Release
    gpg --yes --default-key "$GPG_KEY_ID" -abs -o Release.gpg Release
    echo "    Signed Release for $DISTRO"

    cd "$WORKTREE"
done

# --- Export public GPG key ---
gpg --armor --export "$GPG_KEY_ID" > key.gpg
echo "==> Exported public key to key.gpg"

# --- Create an index page ---
cat > index.html <<HTMLEOF
<!DOCTYPE html>
<html>
<head><title>RISC-V Toolchain APT Repository</title></head>
<body>
<h1>RISC-V Toolchain APT Repository</h1>
<h2>Quick Install</h2>
<pre>
# Add GPG key
curl -fsSL https://makarkul.github.io/riscv-isa-sim/key.gpg | \\
    sudo gpg --dearmor -o /usr/share/keyrings/riscv-toolchain.gpg

# Add repository (replace DISTRO with your Ubuntu version: jammy, noble, etc.)
echo "deb [signed-by=/usr/share/keyrings/riscv-toolchain.gpg] \\
    https://makarkul.github.io/riscv-isa-sim/ DISTRO main" | \\
    sudo tee /etc/apt/sources.list.d/riscv-toolchain.list

# Install
sudo apt update
sudo apt install riscv-toolchain
</pre>
<h2>Available Distributions</h2>
<ul>
$(for d in $DISTROS; do echo "  <li>$d</li>"; done)
</ul>
<h2>Current Version</h2>
<p>$PKG_NAME $PKG_VERSION</p>
<h2>Files</h2>
<ul>
<li><a href="key.gpg">GPG Public Key</a></li>
<li><a href="pool/$COMPONENT/$DEB_BASENAME">$DEB_BASENAME</a></li>
</ul>
</body>
</html>
HTMLEOF

# --- Commit and push ---
git add -A
git commit -m "Publish $PKG_NAME $PKG_VERSION for $DISTROS"
echo "==> Pushing to $BRANCH..."
git push origin "$BRANCH"

# --- Cleanup worktree ---
cd "$SCRIPT_DIR"
git worktree remove "$WORKTREE"

echo ""
echo "==> Published to https://makarkul.github.io/riscv-isa-sim/"
echo ""
echo "Users can install with:"
echo ""
echo "  curl -fsSL https://makarkul.github.io/riscv-isa-sim/key.gpg | \\"
echo "      sudo gpg --dearmor -o /usr/share/keyrings/riscv-toolchain.gpg"
echo ""
echo "  echo \"deb [signed-by=/usr/share/keyrings/riscv-toolchain.gpg] \\"
echo "      https://makarkul.github.io/riscv-isa-sim/ \$(lsb_release -cs) main\" | \\"
echo "      sudo tee /etc/apt/sources.list.d/riscv-toolchain.list"
echo ""
echo "  sudo apt update && sudo apt install riscv-toolchain"
