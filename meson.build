project('drob', ['c', 'cpp'],
        default_options: [
            'buildtype=debugoptimized',
            'warning_level=3',
            'c_std=gnu99',
            'cpp_std=c++14',
        ])

subdir('libs') # Build XED
subdir('include')
subdir('src')

drob_lib = library('drob', drob_src, xed,
                   include_directories: [include_dirs_pub, include_dirs, include_dirs_xed],
                   install: true)
drob = declare_dependency(link_with: drob_lib,
                          include_directories: include_dirs_pub)

subdir('tests')
