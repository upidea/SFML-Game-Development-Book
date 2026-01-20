
cmake_minimum_required(VERSION 3.11)

# Specify default build type if none provided (before project() command)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

# For generators with multiple configurations (like VS), only allow Debug and Release
if(CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Limited configurations" FORCE)
	mark_as_advanced(CMAKE_CONFIGURATION_TYPES)
endif()

# Project name
project(sfml-book)

find_package(SFML COMPONENTS Network Graphics Window Audio System CONFIG REQUIRED)

######################################################################################################################################


# Interprets an argument list separated with keywords.
# Example: 
#  parse_argument_list("MY" "FIRST;SECOND" "FIRST;a;b;c;SECOND;x;y") defines the following variables:
#  MY_FIRST  = a;b;c
#  MY_SECOND = x;y
macro(parse_argument_list PREFIX KEYWORDS ARGUMENTS)
	# Indirection over KEYWORD_LIST necessary because macro parameters are no real variables.
	set(KEYWORD_LIST "${KEYWORDS}")
	set(CURRENT_KEYWORD "")

	# Parse argument list
	foreach(ARG ${ARGUMENTS})
		# See if current argument is in the list of keywords
		list(FIND KEYWORD_LIST "${ARG}" KEYWORD_FOUND)
		if(NOT KEYWORD_FOUND EQUAL -1)
			# If current element is a keyword, store it
			set(CURRENT_KEYWORD ${ARG})
		else()
			# Append current argument to last stored keyword variable
			set(${PREFIX}_${CURRENT_KEYWORD} ${${PREFIX}_${CURRENT_KEYWORD}};${ARG})
		endif()
	endforeach()
endmacro()

######################################################################################################################################

# Macro for chapter building
# Usage:
#  build_chapter(Audio SOURCES First.cpp Second.cpp)
macro(build_chapter CHAPTER_NAME)

	# Parse additional arguments (fills variables CHAPTER_SOURCES and CHAPTER_DEPENDS)
	parse_argument_list("CHAPTER" "SOURCES" "${ARGN}")

	# Status output
	message(STATUS "-> Chapter ${CHAPTER_NAME}")

	#############################################################################
	# Build executable
	project(${CHAPTER_NAME})

	# Executable: Add only Main.cpp, set name without _EXE postfix
	add_executable(${PROJECT_NAME} Main.cpp ${CHAPTER_SOURCES})
	set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${CHAPTER_NAME})

	set(CHAPTER_DIR "${PROJECT_SOURCE_DIR}/..")
	target_include_directories(${PROJECT_NAME} PRIVATE
		"${CHAPTER_DIR}/Include"
		${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
	)

	target_link_libraries(${PROJECT_NAME} PRIVATE
		SFML::Network SFML::Graphics SFML::Window SFML::Audio SFML::System
	)

	# 1. 获取目标的输出目录（自动适配Debug/Release/RelWithDebInfo等配置）
	get_target_property(TARGET_OUTPUT_DIR ${PROJECT_NAME} RUNTIME_OUTPUT_DIRECTORY)
	# 如果未显式设置输出目录，使用默认的二进制目录
	if(NOT TARGET_OUTPUT_DIR)
		# 计算源码目录相对于项目根目录的路径（比如 01_Intro/Source）
		file(RELATIVE_PATH SRC_REL_PATH 
			"${CMAKE_SOURCE_DIR}"  # 项目根源码目录
			"${PROJECT_SOURCE_DIR}"  # 当前章节源码目录：01_Intro/Source
		)
		# 拼接成 exe 实际输出目录：build/01_Intro/Source/Debug
		set(TARGET_OUTPUT_DIR "${CMAKE_BINARY_DIR}/${SRC_REL_PATH}/${CMAKE_BUILD_TYPE}")
	endif()
	# 最终验证：打印最终的目标输出目录
	message(STATUS "👉 最终 Exe 输出目录：${TARGET_OUTPUT_DIR}")

	# 2. 定义Media源目录和目标目录
	set(MEDIA_SOURCE_DIR "${CHAPTER_DIR}/Media")
	set(MEDIA_DEST_DIR "${TARGET_OUTPUT_DIR}/Media")

	# 3. 创建自定义命令：拷贝Media目录（排除CMakeLists.txt）
	add_custom_command(
		OUTPUT ${MEDIA_DEST_DIR}  # 输出标记，避免重复拷贝
		COMMAND ${CMAKE_COMMAND} -E make_directory ${MEDIA_DEST_DIR}  # 确保目标目录存在
		COMMAND ${CMAKE_COMMAND} -E copy_directory
			${MEDIA_SOURCE_DIR}
			${MEDIA_DEST_DIR}
		COMMAND ${CMAKE_COMMAND} -E remove -f ${MEDIA_DEST_DIR}/CMakeLists.txt  # 排除CMakeLists.txt
		DEPENDS ${MEDIA_SOURCE_DIR}  # 源目录变化时重新拷贝
		COMMENT "Copying Media directory to ${MEDIA_DEST_DIR}"
	)

	# 4. 创建自定义目标，并关联到可执行文件目标
	add_custom_target(
		CopyMedia_${CHAPTER_NAME}  # 唯一目标名，避免多章节冲突
		DEPENDS ${MEDIA_DEST_DIR}  # 依赖拷贝命令的输出
	)

	# 5. 强制编译可执行文件时先执行Media拷贝
	add_dependencies(${PROJECT_NAME} CopyMedia_${CHAPTER_NAME})	

	install(TARGETS ${PROJECT_NAME}	RUNTIME DESTINATION ${CHAPTER_NAME})

	install(DIRECTORY ${CHAPTER_DIR}/Media
			DESTINATION ${PROJECT_NAME}
			PATTERN "CMakeLists.txt" EXCLUDE)
endmacro()

# C++ source code, list of all subdirectories
# Must appear after macros, otherwise they are not visible in subdirectories
add_subdirectory(01_Intro/Source)
add_subdirectory(02_Resources/Source)
add_subdirectory(03_World/Source)
add_subdirectory(04_Input/Source)
add_subdirectory(05_States/Source)
add_subdirectory(06_Menus/Source)
add_subdirectory(07_Gameplay/Source)
add_subdirectory(08_Graphics/Source)
add_subdirectory(09_Audio/Source)
add_subdirectory(10_Network/Source)
