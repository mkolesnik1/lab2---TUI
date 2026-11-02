# Ядро: предметная область + сценарии приложения.
# Не линкуется ни с ncurses, ни с SQLite.
add_library(mail_core STATIC
  domain/flags.cpp
  domain/text_util.cpp
  domain/headers.cpp
  domain/header_params.cpp
  domain/codec.cpp
  domain/charset.cpp
  domain/rfc2047.cpp
  domain/date.cpp
  domain/mime.cpp
  domain/summary.cpp
  domain/sorting.cpp
  domain/filters.cpp
  domain/threading.cpp
  application/content_handlers.cpp
  application/message_renderer.cpp
  application/store_registry.cpp
  application/sync_service.cpp
)
target_include_directories(mail_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(mail_core PUBLIC mail_warnings Threads::Threads)
