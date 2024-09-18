add_executable(audio audio.c)
target_link_libraries(audio gral)

add_executable(clipboard clipboard.c)
target_link_libraries(clipboard gral)

add_executable(cursors cursors.c)
target_link_libraries(cursors gral)

add_executable(draw draw.c)
target_link_libraries(draw gral)

add_executable(events events.c)
target_link_libraries(events gral)

add_executable(file file.c)
target_link_libraries(file gral)

add_executable(file_dialogs file_dialogs.c)
target_link_libraries(file_dialogs gral)

add_executable(text text.c)
target_link_libraries(text gral)
