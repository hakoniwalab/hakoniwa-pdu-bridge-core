cmake_minimum_required(VERSION 3.16)

add_executable(bridge_writer bridge_writer.cpp)
add_executable(bridge_reader bridge_reader.cpp)

target_link_libraries(bridge_writer
  PRIVATE
  hakoniwa_pdu_endpoint
)

target_link_libraries(bridge_reader
  PRIVATE
  hakoniwa_pdu_endpoint
)
