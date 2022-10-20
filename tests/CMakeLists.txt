aux_source_directory(. TESTS_SRC_LIST)

foreach(TEST_SRC ${TESTS_SRC_LIST})
  STRING(REGEX REPLACE "^\./|\.cpp$" "" TEST_EXE_NAME ${TEST_SRC}) # 删除TEST_SRC前端的./和末尾的.cpp
  message("添加测试程序：${TEST_EXE_NAME}")
  add_executable(${TEST_EXE_NAME} ${TEST_SRC} )
  target_link_libraries(${TEST_EXE_NAME} ${PROJECT_NAME}_static)
  
endforeach(TEST_SRC ${TESTS_SRC_LIST})
