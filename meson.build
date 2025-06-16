#   Copyright (C) 2025 Cynthia
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <https://www.gnu.org/licenses/>.

project('vsp', 'c', version : '1.0')

cmake = import('cmake')
cmake_opts = cmake.subproject_options()
cmake_opts.add_cmake_defines({
    'KISSFFT_STATIC': true,
    'KISSFFT_TEST': false,
    'KISSFFT_TOOLS': false
})
kissfft = cmake.subproject('kissfft', options : cmake_opts)

deps = [kissfft.dependency('kissfft'),
        dependency('glfw3'),
        dependency('libpipewire-0.3'),]

cc = meson.get_compiler('c')
deps += [cc.find_library('m', required : false)]

executable('vsp', sources : ['vsp.c', 'pipewire.c', 'renderer.c', 'gl.c'], dependencies : deps)
