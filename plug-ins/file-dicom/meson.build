if have_gdcm
  plugin_name = 'file-dicom'

  plugin_sources = [
    'file-dicom.c',
    'gdcm-wrapper.cc',
  ]

  gdcm_jpeg12_dep = meson.get_compiler('cpp').find_library('libgdcmjpeg12', required: true)
  gdcm_common_dep = meson.get_compiler('cpp').find_library('libgdcmCommon', required: true)
  gdcm_jpeg16_dep = meson.get_compiler('cpp').find_library('libgdcmjpeg16', required: true)
  gdcm_dict_dep = meson.get_compiler('cpp').find_library('libgdcmDICT', required: true)
  gdcm_jpeg8_dep = meson.get_compiler('cpp').find_library('libgdcmjpeg8', required: true)
  gdcm_dsed_dep = meson.get_compiler('cpp').find_library('libgdcmDSED', required: true)
  gdcm_msff_dep = meson.get_compiler('cpp').find_library('libgdcmMSFF', required: true)
  gdcm_iod_dep = meson.get_compiler('cpp').find_library('libgdcmIOD', required: true)
  gdcm_mexd_dep = meson.get_compiler('cpp').find_library('libgdcmMEXD', required: true)

  gdcm_dependencies = [
    libgimpui_dep,
    gdcm_jpeg12_dep,
    gdcm_common_dep,
    gdcm_jpeg16_dep,
    gdcm_dict_dep,
    gdcm_jpeg8_dep,
    gdcm_dsed_dep,
    gdcm_msff_dep,
    gdcm_iod_dep,
    gdcm_mexd_dep,
  ]

  if platform_windows
    gdcm_charls_dep = meson.get_compiler('cpp').find_library('libgdcmcharls', required: true)
    gdcm_dependencies += gdcm_charls_dep
  endif

  if platform_windows
    plugin_sources += windows.compile_resources(
      gimp_plugins_rc,
      args: [
        '--define', 'ORIGINALFILENAME_STR="@0@"'.format(plugin_name+'.exe'),
        '--define', 'INTERNALNAME_STR="@0@"'    .format(plugin_name),
        '--define', 'TOP_SRCDIR="@0@"'          .format(meson.project_source_root()),
      ],
      include_directories: [
        rootInclude, appInclude,
      ],
    )
  endif

  plugin_exe = executable(plugin_name,
                          plugin_sources,
                          dependencies: gdcm_dependencies,
                          win_subsystem: 'windows',
                          install: true,
                          install_dir: gimpplugindir / 'plug-ins' / plugin_name,
                        )
  plugin_executables += [plugin_exe.full_path()]
endif