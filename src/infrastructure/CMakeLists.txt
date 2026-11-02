# Инфраструктура: реализации портов ядра (Maildir, SQLite-индекс, iconv).
add_library(mail_infrastructure STATIC
  iconv_converter.cpp
)
target_include_directories(mail_infrastructure PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(mail_infrastructure PUBLIC mail_core)
