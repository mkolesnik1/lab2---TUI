# Правила зависимостей между слоями (только вниз):
#   core/domain      -> (ничего из проекта, кроме самого domain)
#   core/application -> core/domain, core/application
#   infrastructure   -> core
#   presentation     -> core
# Запуск: cmake -DSRC=<путь к src> -P check_layers.cmake
set(violations "")

function(check_dir dir forbidden_regex layer)
  file(GLOB_RECURSE files "${SRC}/${dir}/*.h" "${SRC}/${dir}/*.cpp")
  foreach(f ${files})
    file(STRINGS "${f}" lines REGEX "^#include \"")
    foreach(line ${lines})
      if(line MATCHES "${forbidden_regex}")
        set(violations "${violations}\n  ${layer}: ${f}: ${line}")
      endif()
    endforeach()
  endforeach()
  set(violations "${violations}" PARENT_SCOPE)
endfunction()

check_dir(core/domain "\"(core/application|infrastructure|presentation)/" "domain")
check_dir(core/application "\"(infrastructure|presentation)/" "application")
check_dir(infrastructure "\"presentation/" "infrastructure")
check_dir(presentation "\"infrastructure/" "presentation")

if(violations)
  message(FATAL_ERROR "Нарушены зависимости между слоями:${violations}")
endif()
message(STATUS "Слои соблюдены")
