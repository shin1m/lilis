add_executable(lilis objects.cc engine.cc code.cc builtins.cc main.cc)
target_compile_features(lilis PUBLIC cxx_std_17)
