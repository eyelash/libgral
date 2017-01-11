add_executable(clipboard clipboard.c)
target_link_libraries(clipboard gral)

add_executable(file_dialogs file_dialogs.c)
target_link_libraries(file_dialogs gral)

add_executable(gradients gradients.c)
target_link_libraries(gradients gral)

add_executable(text text.c)
target_link_libraries(text gral)
