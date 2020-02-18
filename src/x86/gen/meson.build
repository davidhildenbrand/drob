x86_gendefs = executable('x86_gendefs', files('gen-defs.cpp'),
                         include_directories: include_dirs,
                         native: true)
x86_gendefs_h = custom_target('x86_gendefs_h', command: [x86_gendefs],
                              capture: true, output: 'gen-defs.h')

drob_src += [x86_gendefs_h]
