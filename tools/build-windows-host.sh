#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work="$repo/.build-host-tools/repro-$(date +%s)"
out="$repo/dist/windows-host"
mkdir -p "$work" "$out"

git clone --quiet --depth 1 --branch libnfc-1.8.0 https://github.com/nfc-tools/libnfc.git "$work/libnfc"
git clone --quiet --depth 1 --branch mfoc-0.10.7 https://github.com/nfc-tools/mfoc.git "$work/mfoc"
[[ "$(git -C "$work/libnfc" rev-parse HEAD)" == f02ff51449240102c27a97173dc495e8e7789046 ]] || { echo 'Unexpected libnfc source revision' >&2; exit 1; }
[[ "$(git -C "$work/mfoc" rev-parse HEAD)" == 290a0759567d4df4840d9133d663ac88246b373a ]] || { echo 'Unexpected mfoc source revision' >&2; exit 1; }

git -C "$work/libnfc" apply "$repo/tools/patches/libnfc-win-stdlib.patch"
git -C "$work/mfoc" apply "$repo/tools/patches/mfoc-win-errno.patch"

cat > "$work/mingw-toolchain.cmake" <<'EOF'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
EOF

cmake -S "$work/libnfc" -B "$work/libnfc-build" \
    -DCMAKE_TOOLCHAIN_FILE="$work/mingw-toolchain.cmake" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_INSTALL_PREFIX="$work/prefix" \
    -DLIBNFC_SYSCONFDIR="$work/prefix/config" \
    -DDLLTOOL="$(command -v x86_64-w64-mingw32-dlltool)" \
    -DLIBNFC_DRIVER_ACR122_USB=OFF \
    -DLIBNFC_DRIVER_PN53X_USB=OFF \
    -DLIBNFC_DRIVER_ACR122S=OFF \
    -DLIBNFC_DRIVER_ARYGON=OFF \
    -DLIBNFC_CONFFILES_MODE=ON \
    -DLIBNFC_LOG=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_UTILS=ON
cmake --build "$work/libnfc-build" --parallel 4

cat > "$work/libnfc.pc" <<EOF
prefix=$work
libdir=$work/libnfc-build/libnfc
includedir=$work/libnfc/include
Name: libnfc
Description: Near Field Communication library
Version: 1.8.0
Libs: -L\${libdir} -lnfc
Cflags: -I\${includedir}
EOF

(cd "$work/mfoc" && autoreconf -fis)
mkdir -p "$work/mfoc-build"
(cd "$work/mfoc-build" && \
    PKG_CONFIG_PATH="$work" CC=x86_64-w64-mingw32-gcc \
    "$work/mfoc/configure" --host=x86_64-w64-mingw32 --prefix="$work/prefix")
make -C "$work/mfoc-build" -j4 CPPFLAGS="-I$repo/tools/compat"

cp "$work/libnfc-build/libnfc/libnfc.dll" "$out/"
cp "$work/libnfc-build/utils/nfc-list.exe" "$out/"
cp "$work/libnfc-build/utils/nfc-mfclassic.exe" "$out/"
cp "$work/mfoc-build/src/mfoc.exe" "$out/"
cp "$work/libnfc/COPYING" "$out/LIBNFC-LICENSE.txt"
cp "$work/mfoc/COPYING" "$out/MFOC-LICENSE.txt"
git -C "$work/libnfc" archive --format=tar HEAD | gzip -9 > "$out/libnfc-1.8.0-source.tar.gz"
git -C "$work/mfoc" archive --format=tar HEAD | gzip -9 > "$out/mfoc-0.10.7-source.tar.gz"
cp "$repo/tools/patches/libnfc-win-stdlib.patch" "$out/"
cp "$repo/tools/patches/mfoc-win-errno.patch" "$out/"
cp "$repo/tools/compat/err.h" "$out/MFOC-win-err.h"
cp "$repo/tools/build-windows-host.sh" "$out/"
cat > "$out/SOURCE-REVISIONS.txt" <<'EOF'
libnfc 1.8.0: https://github.com/nfc-tools/libnfc
commit: f02ff51449240102c27a97173dc495e8e7789046
mfoc 0.10.7: https://github.com/nfc-tools/mfoc
commit: 290a0759567d4df4840d9133d663ac88246b373a
Apply the included compatibility patches to the source archives.
The build procedure is tools/build-windows-host.sh in the Bruce PN532 Field Kit repository.
EOF
(cd "$out" && sha256sum *.exe *.dll *.tar.gz *.patch *.h *.sh > SHA256SUMS.txt)
echo "Windows host tools: $out"
