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
include CMakeFiles/example.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/example.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/example.dir/flags.make

CMakeFiles/example.dir/example.cpp.o: CMakeFiles/example.dir/flags.make
CMakeFiles/example.dir/example.cpp.o: example.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/okie90/witcher/third_party/P-ART/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/example.dir/example.cpp.o"
	/home/okie90/llvm/llvm-9.0.1.src/build/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/example.dir/example.cpp.o -c /home/okie90/witcher/third_party/P-ART/example.cpp

CMakeFiles/example.dir/example.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/example.dir/example.cpp.i"
	/home/okie90/llvm/llvm-9.0.1.src/build/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/okie90/witcher/third_party/P-ART/example.cpp > CMakeFiles/example.dir/example.cpp.i

CMakeFiles/example.dir/example.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/example.dir/example.cpp.s"
	/home/okie90/llvm/llvm-9.0.1.src/build/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/okie90/witcher/third_party/P-ART/example.cpp -o CMakeFiles/example.dir/example.cpp.s

CMakeFiles/example.dir/Epoche.cpp.o: CMakeFiles/example.dir/flags.make
CMakeFiles/example.dir/Epoche.cpp.o: Epoche.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/okie90/witcher/third_party/P-ART/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/example.dir/Epoche.cpp.o"
	/home/okie90/llvm/llvm-9.0.1.src/build/bin/clang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/example.dir/Epoche.cpp.o -c /home/okie90/witcher/third_party/P-ART/Epoche.cpp

CMakeFiles/example.dir/Epoche.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/example.dir/Epoche.cpp.i"
	/home/okie90/llvm/llvm-9.0.1.src/build/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/okie90/witcher/third_party/P-ART/Epoche.cpp > CMakeFiles/example.dir/Epoche.cpp.i

CMakeFiles/example.dir/Epoche.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/example.dir/Epoche.cpp.s"
	/home/okie90/llvm/llvm-9.0.1.src/build/bin/clang++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/okie90/witcher/third_party/P-ART/Epoche.cpp -o CMakeFiles/example.dir/Epoche.cpp.s

# Object files for target example
example_OBJECTS = \
"CMakeFiles/example.dir/example.cpp.o" \
"CMakeFiles/example.dir/Epoche.cpp.o"

# External object files for target example
example_EXTERNAL_OBJECTS =

example: CMakeFiles/example.dir/example.cpp.o
example: CMakeFiles/example.dir/Epoche.cpp.o
example: CMakeFiles/example.dir/build.make
example: libpart.a
example: /usr/lib64/libjemalloc.so
example: /usr/lib64/libtbb.so
example: CMakeFiles/example.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/okie90/witcher/third_party/P-ART/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable example"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/example.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/example.dir/build: example

.PHONY : CMakeFiles/example.dir/build

CMakeFiles/example.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/example.dir/cmake_clean.cmake
.PHONY : CMakeFiles/example.dir/clean

CMakeFiles/example.dir/depend:
	cd /home/okie90/witcher/third_party/P-ART && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/okie90/witcher/third_party/P-ART /home/okie90/witcher/third_party/P-ART /home/okie90/witcher/third_party/P-ART /home/okie90/witcher/third_party/P-ART /home/okie90/witcher/third_party/P-ART/CMakeFiles/example.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/example.dir/depend

