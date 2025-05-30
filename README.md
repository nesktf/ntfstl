# ntfstl
C++20 standard library for my projects.

# Usage inside projects
Only tested on Debian 12 (using GCC 13).

You will need the following packages installed in your system:
- gcc 13 
- libfmt-dev (only tested on 9.1.0)

```sh
sudo apt install gcc libfmt-dev
```

Clone the repository inside your project
```sh
$ cd my_funny_project/
$ mkdir -p lib/
$ git clone https://github.com/nesktf/ntfstl.git ./lib/ntfstl
```

And then add the following in your CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.11)
project(my_funny_project CXX)
# ...
add_subdirectory("lib/ntfstl")
# ...
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
target_link_libraries(${PROJECT_NAME} ntfstl)

```

# Tests
If you want to run tests, use the following command
```sh
$ cmake -B build -DNTFSTL_BUILD_TESTS=1
$ make -C build -j4
$ ctest --test-dir build/test/ -V --progress
```
