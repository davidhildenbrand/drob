python = find_program('python', native: true)
xed = custom_target('xed', command: [python,
                                     files('xed/mfile.py'), '--verbose=3',
                                     '--extra-flags', '-fPIC', '--no-encoder',
                                     '--limit-strings', '--no-amd', '--no-mpx',
                                     '--build-dir=@OUTDIR@',
                                     ],
                    output: ['libxed.a'],
                    console: true)

include_dirs_xed = include_directories('xed/include/public/xed/')
