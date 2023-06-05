libapperrorlog_sources = [
  'error_log.c',
]

libapperrorlog = static_library('apperrorlog',
  libapperrorlog_sources,
  include_directories: [ rootInclude, rootAppInclude, ],
  c_args: '-DG_LOG_DOMAIN="Gimp-File-Data"',
  dependencies: [
    gtk3,
  ],
)