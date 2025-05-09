project('vsp', 'c')

deps = [dependency('kissfft-float'),
        dependency('sdl3'),
        dependency('glfw3'),
        dependency('libpipewire-0.3')]

cc = meson.get_compiler('c')
deps += [cc.find_library('m', required : false)]

executable('vsp', sources : ['vsp.c', 'pw.c', 'pr.c', 'gl.c'], dependencies : deps)
