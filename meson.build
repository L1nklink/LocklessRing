project('LocklessRing', 'cpp', default_options:['cpp_std=c++20'])

sources = files()

includes = include_directories()

cpp = meson.get_compiler('cpp')

depends = []
depends += dependency('threads')

add_project_arguments('-DPBUILDVERSION="meson-build"', language:'cpp')
add_project_arguments('-w', language:'cpp')

depends += declare_dependency(sources:sources, include_directories:includes, dependencies:depends)
executable('a.out', sources+files('main.cc'), include_directories:includes, dependencies:depends)
