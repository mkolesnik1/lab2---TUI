if(TARGET mail_presentation)
  add_executable(maills main.cpp)
  target_link_libraries(maills PRIVATE mail_presentation mail_infrastructure)
endif()
