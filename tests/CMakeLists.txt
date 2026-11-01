# Тесты ядра и инфраструктуры. Без ncurses: ядро проверяется отдельно от TUI.
add_executable(mail_tests
  main.cpp
  test_text.cpp
  test_codec.cpp
  test_charset.cpp
  test_rfc2047.cpp
  test_mime.cpp
  test_threads.cpp)
target_link_libraries(mail_tests PRIVATE mail_infrastructure)

# Один ctest-тест на набор (suite)
foreach(suite text codec charset rfc2047 mime threads)
  add_test(NAME ${suite} COMMAND mail_tests ${suite})
endforeach()

# Проверка слоёв: ядро не включает заголовки infrastructure/presentation и т.д.
add_test(NAME layering
  COMMAND ${CMAKE_COMMAND} -DSRC=${PROJECT_SOURCE_DIR}/src -P ${CMAKE_CURRENT_SOURCE_DIR}/check_layers.cmake)
