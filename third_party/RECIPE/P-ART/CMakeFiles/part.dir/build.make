# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.14

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/okie90/witcher/third_party/P-ART

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/okie90/witcher/third_party/P-ART

# Include any dependencies generated for this target.
include CMakeFiles/part.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/part.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/part.dir/flags.make

CMakeFiles/part.dir/Tree.cpp.o: CMakeFiles/part.dir/flags.make
CMakeFiles/part.dir/Tree.cpp.o: Tree.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/okie90/witcher/third_party/P-ART/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/part.dir/Tree.cpp.o"
	/home/okie90/llvm/llvm-9.0.1.src/build/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/part.dir/Tree.cpp.o -c /home/okie90/witcher/third_party/P-ART/Tree.cpp

CMakeFiles/part.dir/Tree.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/part.dir/Tree.cpp.i"
	/home/okie90/llvm/llvm-9.0.1.src/build/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/okie90/witcher/third_party/P-ART/Tree.cpp > CMakeFiles/part.dir/Tree.cpp.i

CMakeFiles/part.dir/Tree.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/part.dir/Tree.cpp.s"
	/home/okie90/llvm/llvm-9.0.1.src/build/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/okie90/witcher/third_party/P-ART/Tree.cpp -o CMakeFiles/part.dir/Tree.cpp.s

CMakeFiles/part.dir/pmdk.cpp.o: CMakeFiles/part.dir/flags.make
CMakeFiles/part.dir/pmdk.cpp.o: pmdk.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/okie90/witcher/third_party/P-ART/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/part.dir/pmdk.cpp.o"
	/home/okie90/llvm/llvm-9.0.1.src/build/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/part.dir/pmdk.cpp.o -c /home/okie90/witcher/third_party/P-ART/pmdk.cpp

CMakeFiles/part.dir/pmdk.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/part.dir/pmdk.cpp.i"
	/home/okie90/llvm/llvm-9.0.1.src/build/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/okie90/witcher/third_party/P-ART/pmdk.cpp > CMakeFiles/part.dir/pmdk.cpp.i

CMakeFiles/part.dir/pmdk.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/part.dir/pmdk.cpp.s"
	/home/okie90/llvm/llvm-9.0.1.src/build/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/okie90/witcher/third_party/P-ART/pmdk.cpp -o CMakeFiles/part.dir/pmdk.cpp.s

# Object files for target part
part_OBJECTS = \
"CMakeFiles/part.dir/Tree.cpp.o" \
"CMakeFiles/part.dir/pmdk.cpp.o"

# External object files for target part
part_EXTERNAL_OBJECTS =

libpart.a: CMakeFiles/part.dir/Tree.cpp.o
libpart.a: CMakeFiles/part.dir/pmdk.cpp.o
libpart.a: CMakeFiles/part.dir/build.make
libpart.a: CMakeFiles/part.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/okie90/witcher/third_party/P-ART/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX static library libpart.a"
	$(CMAKE_COMMAND) -P CMakeFiles/part.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/part.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/part.dir/build: libpart.a

.PHONY : CMakeFiles/part.dir/build

CMakeFiles/part.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/part.dir/cmake_clean.cmake
.PHONY : CMakeFiles/part.dir/clean

CMakeFiles/part.dir/depend:
	cd /home/okie90/witcher/third_party/P-ART && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/okie90/witcher/third_party/P-ART /home/okie90/witcher/third_party/P-ART /home/okie90/witcher/third_party/P-ART /home/okie90/witcher/third_party/P-ART /home/okie90/witcher/third_party/P-ART/CMakeFiles/part.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/part.dir/depend

