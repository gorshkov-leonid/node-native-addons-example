{
  "targets": [
    {
      "target_name": "screener",
      "cflags!": [
        "-fno-exceptions",
        "-std=-std=c++17",
        "-stdlib=libc++"
      ],
      "cflags_cc!": [
        "-fno-exceptions",
        "-std=-std=c++17",
        "-stdlib=libc++",
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          "ExceptionHandling": 1,
          'AdditionalOptions': [
            '-std:c++17'
          ]
        }
      },
      "sources": [ "screener.cc"],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ],
    }
  ]
}