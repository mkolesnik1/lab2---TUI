# Инфраструктура: реализации портов ядра (Maildir, SQLite-индекс, iconv).
find_package(SQLite3 REQUIRED)

add_library(mail_infrastructure STATIC
  maildir_store.cpp
  iconv_converter.cpp
  sqlite_index.cpp
  builtin_stores.cpp
)
target_include_directories(mail_infrastructure PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(mail_infrastructure PUBLIC mail_core SQLite::SQLite3)
