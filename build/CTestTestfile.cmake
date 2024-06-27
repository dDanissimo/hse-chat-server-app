# CMake generated Testfile for 
# Source directory: /home/archangel/ChatServerClient
# Build directory: /home/archangel/ChatServerClient/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_client "/home/archangel/ChatServerClient/build/test_client")
set_tests_properties(test_client PROPERTIES  _BACKTRACE_TRIPLES "/home/archangel/ChatServerClient/CMakeLists.txt;42;add_test;/home/archangel/ChatServerClient/CMakeLists.txt;0;")
add_test(test_server "/home/archangel/ChatServerClient/build/test_server")
set_tests_properties(test_server PROPERTIES  _BACKTRACE_TRIPLES "/home/archangel/ChatServerClient/CMakeLists.txt;47;add_test;/home/archangel/ChatServerClient/CMakeLists.txt;0;")
subdirs("_deps/json-build")
