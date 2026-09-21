project('bluray-menu-probe', 'c', version: '0.1.0', default_options: ['c_std=c11', 'warning_level=3'])
cc = meson.get_compiler('c')
prefix = meson.project_source_root() / '../out/libbluray-1.5.0-x64'
bluray = cc.find_library('bluray', dirs: prefix / 'lib')
executable('bluray-rle-probe', 'bluray-rle-probe.c',
    include_directories: include_directories('../out/libbluray-1.5.0-x64/include'),
    dependencies: bluray,
    c_args: ['-D_CRT_SECURE_NO_WARNINGS'], install: false)
executable('bluray-menu-probe', 'bluray-menu-probe.c',
    include_directories: include_directories('../out/libbluray-1.5.0-x64/include'),
    dependencies: bluray,
    c_args: ['-D_CRT_SECURE_NO_WARNINGS'],
    install: false)
executable('bluray-mouse-probe', 'bluray-mouse-probe.c',
    include_directories: include_directories('../out/libbluray-1.5.0-x64/include'),
    dependencies: bluray,
    c_args: ['-D_CRT_SECURE_NO_WARNINGS'],
    install: false)
executable('bluray-bdj-probe', 'bluray-bdj-probe.c',
    include_directories: include_directories('../out/libbluray-1.5.0-x64/include'),
    dependencies: bluray,
    c_args: ['-D_CRT_SECURE_NO_WARNINGS'],
    install: false)
