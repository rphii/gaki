
sources = [
  'main.c'
  ]

rlc_dep = dependency('rlc', fallback : ['rlc', 'rlc_dep'], default_options: ['default_library=static'])
rlso_dep = dependency('rlso', fallback : ['rlso', 'rlso_dep'], default_options: ['default_library=static'])
rltui_dep = dependency('rltui', fallback : ['rltui', 'rltui_dep'], default_options: ['default_library=static'])

gaki = executable('gaki',
  sources,
  dependencies: [rlc_dep, rlso_dep, rltui_dep],
  install: true,
  )
