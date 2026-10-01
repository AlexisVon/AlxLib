# Copyright (c) 2026 AlexisVon
# SPDX-License-Identifier: MIT
# See LICENSE file in the project root for full license information.

{
  "targets": [
    {
      "target_name": "alxbase",
      "sources": [
        "../../source/alxbase/aalgo.cpp",
        "../../source/alxbase/abytes.cpp",
        "../../source/alxbase/ajson.cpp",
        "../../source/alxbase/axml.cpp",
        "../../source/alxbase/astring.cpp",
        "../../source/alxbase/aregex_ex.cpp",
        "../../source/alxbase/avarsolid.cpp",
        "source/interface.cpp",
        "source/args_wrap.cpp",
        "source/varsolid_node.cpp"
      ],
      "include_dirs": [
        "include",
        "../../include",
        "../../include/alxbase"
      ],
      "product_dir": "<(module_root_dir)/bin",
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "defines": ["ALEXISLIB_STATIC=1"],
      "conditions": [
        ["OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "RuntimeTypeInfo": "true",
              "Optimization": "3",
              "PreprocessorDefinitions": ["NDEBUG"]
            }
          }
        }, {
          "cflags_cc": ["-Wall", "-frtti", "-fexceptions", "-std=c++20"],
          "configurations": {
            "Release": {
              "cflags_cc": ["-O3", "-DNDEBUG", "-fno-omit-frame-pointer", "-g"],
              "ldflags": ["-O3"]
            },
            "Debug": {
              "cflags_cc": ["-O0", "-g", "-DDEBUG"],
              "ldflags": ["-O0"]
            }
          }
        }]
      ]
    },
    {
      "target_name": "extract_debug_info",
      "type": "none",
      "dependencies": ["alxbase"],
      "conditions": [
        ["OS!='win'", {
          "actions": [
            {
              "action_name": "process_debug_symbols",
              "inputs": ["<(module_root_dir)/bin/alxbase.node"],
              "outputs": ["<(module_root_dir)/bin/alxbase.node.debug"],
              "action": [
                "sh", "-c",
                "objcopy --only-keep-debug <(module_root_dir)/bin/alxbase.node <(module_root_dir)/bin/alxbase.node.debug && " +
                "objcopy --strip-debug <(module_root_dir)/bin/alxbase.node && " +
                "objcopy --add-gnu-debuglink=<(module_root_dir)/bin/alxbase.node.debug <(module_root_dir)/bin/alxbase.node"
              ]
            }
          ]
        }]
      ]
    }
  ]
}
