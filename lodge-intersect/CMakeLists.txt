# lodge-intersect

add_library(lodge-intersect STATIC "")

target_sources(lodge-intersect
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/intersect.c"
        "${CMAKE_CURRENT_LIST_DIR}/collide.c"
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/intersect.h"
        "${CMAKE_CURRENT_LIST_DIR}/collide.h"
)

target_include_directories(lodge-intersect
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(lodge-intersect
	PRIVATE
		lodge-build-flags
		lodge-lib
)
