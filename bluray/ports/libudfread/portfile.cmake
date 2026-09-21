# Based on microsoft/vcpkg d015e31e90838a4c9dfa3eed45979bc70d9357fc.
# Keep its MSVC patch; use VideoLAN's published release instead of the
# GitLab-generated archive, which can return an HTML bot challenge in CI.
# The vcpkg port is MIT-licensed; see ../LICENSE.txt.
vcpkg_download_distfile(ARCHIVE
    URLS "https://download.videolan.org/pub/videolan/libudfread/libudfread-${VERSION}.tar.xz"
    FILENAME "libudfread-${VERSION}.tar.xz"
    SHA512 e3ed8dc7fab472ad382b1b6cd068f1dc0084c34e5ec2c460c1dd84fa14a9d368c4d7af08a23efc5057e7f021f5812222c01247d5f304632716c94fc8c689a1a2
)

vcpkg_extract_source_archive(SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
    PATCHES msvc.diff
)

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -Denable_examples=false
)

vcpkg_install_meson()
vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
