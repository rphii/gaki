
sources = [
  'main.c',
  'file-info.c',
  'panel-gaki.c',
  'nav-directory.c',
  'gaki.c',
  ]

cc = meson.get_compiler('c')

m_dep = cc.find_library('m', required : false)
rlc_dep = dependency('rlc', fallback : ['rlc', 'rlc_dep'], default_options: ['default_library=static'])
rlso_dep = dependency('rlso', fallback : ['rlso', 'rlso_dep'], default_options: ['default_library=static'])
rlpw_dep = dependency('rlpw', fallback : ['rlpw', 'rlpw_dep'], default_options: ['default_library=static'])
rltui_dep = dependency('rltui', fallback : ['rltui', 'rltui_dep'], default_options: ['default_library=static'])
rlwcwidth_dep = dependency('rlwcwidth', fallback : ['rlwcwidth', 'rlwcwidth_dep'], default_options: ['default_library=static'])

gaki = executable('gaki',
  sources,
  dependencies: [m_dep, rlc_dep, rlso_dep, rlpw_dep, rltui_dep, rlwcwidth_dep],
  install: true,
  )
