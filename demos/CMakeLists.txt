add_executable(gradients gradients.c)
target_link_libraries(gradients gral)

add_executable(text text.c)
target_link_libraries(text gral)
