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

executable('vsp', sources : ['vsp.c', 'pw.c', 'pr.c', 'gl.c'], dependencies : deps)
