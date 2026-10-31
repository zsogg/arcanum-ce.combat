#!/usr/bin/env bash
# Builds 7zz, 7-Zip's standalone console program, for Arcanum CE's macOS
# package, from 7-Zip's own source release with patches/ applied: an Intel
# slice for macOS 10.13 and an Apple Silicon slice for 11.0, joined into one
# universal binary and signed ad hoc, in build/7zz, with 7-Zip's licence
# texts beside it. Run it on a Mac with Xcode's command line tools.
#
# 7-Zip's prebuilt Mac binary is not used: its Intel slice requires macOS
# 10.15 (patches/ says why), and the package promises 10.13.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
HERE="$PWD"
VERSION=26.05
FILE=7z2605-src.tar.xz
URL="https://github.com/ip7z/7zip/releases/download/$VERSION/$FILE"
SHA256=0b6080b8c3e7a579161d2d495224c0e51c467576cb4c7048b0ab485ef44cc22e

rm -rf build
mkdir -p build/src
curl -fsSL --retry 3 -o "build/$FILE" "$URL"
echo "$SHA256  build/$FILE" | shasum -a 256 -c -
tar -xJf "build/$FILE" -C build/src
chmod -R u+w build/src
for patch in "$HERE"/patches/*.patch; do
    patch -p1 -t -F 0 -d build/src -i "$patch"
done

# Any call of an API newer than the target is an error rather than a weak
# link that crashes on the older macOS; and no build path in the binary.
STRICT="-Werror=unguarded-availability -Werror=unguarded-availability-new -ffile-prefix-map=$HERE/build/src=7zip"
JOBS="$(sysctl -n hw.ncpu)"
cd build/src/CPP/7zip/Bundles/Alone2
make -f ../../cmpl_mac_x64.mak -j"$JOBS" MY_ARCH="-arch x86_64 -mmacosx-version-min=10.13 $STRICT"
make -f ../../cmpl_mac_arm64.mak -j"$JOBS" MY_ARCH="-arch arm64 -march=armv8-a -mmacosx-version-min=11.0 $STRICT"
lipo -create -output "$HERE/build/7zz" b/m_x64/7zz b/m_arm64/7zz
# lipo's output carries no signature, and macOS kills an unsigned arm64 binary.
codesign --force -s - "$HERE/build/7zz"
cp "$HERE/build/src/DOC/License.txt" "$HERE/build/src/DOC/unRarLicense.txt" "$HERE/build/"
echo "7zz: 7-Zip $VERSION from $URL (sha256 $SHA256), with tools/7zip/patches/." > "$HERE/build/SOURCE.txt"
