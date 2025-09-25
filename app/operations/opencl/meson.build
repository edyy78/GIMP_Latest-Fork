cltostring = files('cltostring.py')

opencl_sources = [
  'set-alpha.cl',
]

opencl_headers = files()
foreach source : opencl_sources
  opencl_headers += custom_target(source + '.h',
    input : source,
    output: source +'.h',
    command: [ python, cltostring, '@INPUT@', '@OUTPUT@' ],
  )
endforeach
