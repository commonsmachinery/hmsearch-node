{
    'targets': [{
        'target_name': 'hmsearch',
        'sources': [ 'ext/hmnode.cc', 'hmsearch/hmsearch.cc' ],
        'include_dirs': [
            'hmsearch',
            "<!(node -e \"require('nan')\")",
        ],
        'conditions': [
            ['OS=="linux"', {
                'cflags': [ '-g', '-Wall' ],
                'cflags_cc': [ '-g', '-Wall' ],
                'cflags!': [ '-fno-exceptions', '-fno-rtti' ],
                'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
                'libraries': [
                    '-lkyotocabinet',
                ],
            }],
        ],
    }],
}
