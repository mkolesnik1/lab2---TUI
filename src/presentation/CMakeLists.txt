# Представление: только ncurses и отрисовка. Зависит от ядра, но не от infrastructure.
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses)

if(CURSES_FOUND)
  add_library(mail_presentation STATIC terminal.cpp views.cpp app.cpp)
  target_include_directories(mail_presentation PUBLIC ${PROJECT_SOURCE_DIR}/src ${CURSES_INCLUDE_DIRS})
  target_link_libraries(mail_presentation PUBLIC mail_core ${CURSES_LIBRARIES})
else()
  message(WARNING "ncurses (wide) не найден: TUI не будет собран. Установите libncurses-dev.")
endif()
