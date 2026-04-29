# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "D:/ESP/.espressif/v6.0.1/esp-idf/components/bootloader/subproject")
  file(MAKE_DIRECTORY "D:/ESP/.espressif/v6.0.1/esp-idf/components/bootloader/subproject")
endif()
file(MAKE_DIRECTORY
  "D:/Mathias Lampert/Mathias Schule_Studium/FH/ELITE-Semester/EIT_3/Embedded-BuildingBlocks/Projekt/Tetris/TetrisCode/build/bootloader"
  "D:/Mathias Lampert/Mathias Schule_Studium/FH/ELITE-Semester/EIT_3/Embedded-BuildingBlocks/Projekt/Tetris/TetrisCode/build/bootloader-prefix"
  "D:/Mathias Lampert/Mathias Schule_Studium/FH/ELITE-Semester/EIT_3/Embedded-BuildingBlocks/Projekt/Tetris/TetrisCode/build/bootloader-prefix/tmp"
  "D:/Mathias Lampert/Mathias Schule_Studium/FH/ELITE-Semester/EIT_3/Embedded-BuildingBlocks/Projekt/Tetris/TetrisCode/build/bootloader-prefix/src/bootloader-stamp"
  "D:/Mathias Lampert/Mathias Schule_Studium/FH/ELITE-Semester/EIT_3/Embedded-BuildingBlocks/Projekt/Tetris/TetrisCode/build/bootloader-prefix/src"
  "D:/Mathias Lampert/Mathias Schule_Studium/FH/ELITE-Semester/EIT_3/Embedded-BuildingBlocks/Projekt/Tetris/TetrisCode/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Mathias Lampert/Mathias Schule_Studium/FH/ELITE-Semester/EIT_3/Embedded-BuildingBlocks/Projekt/Tetris/TetrisCode/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Mathias Lampert/Mathias Schule_Studium/FH/ELITE-Semester/EIT_3/Embedded-BuildingBlocks/Projekt/Tetris/TetrisCode/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
