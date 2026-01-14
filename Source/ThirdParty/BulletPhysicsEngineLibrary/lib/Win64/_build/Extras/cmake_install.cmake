# Install script for directory: F:/UnrealProjects/AntiHero/BulletPhysics/Plugins/BulletNPP/Source/ThirdParty/BulletPhysicsEngineLibrary/bullet3/Extras

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files/BULLET_PHYSICS")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("F:/UnrealProjects/AntiHero/BulletPhysics/Plugins/BulletNPP/Source/ThirdParty/BulletPhysicsEngineLibrary/lib/Win64/_build/Extras/InverseDynamics/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("F:/UnrealProjects/AntiHero/BulletPhysics/Plugins/BulletNPP/Source/ThirdParty/BulletPhysicsEngineLibrary/lib/Win64/_build/Extras/BulletRoboticsGUI/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("F:/UnrealProjects/AntiHero/BulletPhysics/Plugins/BulletNPP/Source/ThirdParty/BulletPhysicsEngineLibrary/lib/Win64/_build/Extras/BulletRobotics/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("F:/UnrealProjects/AntiHero/BulletPhysics/Plugins/BulletNPP/Source/ThirdParty/BulletPhysicsEngineLibrary/lib/Win64/_build/Extras/obj2sdf/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("F:/UnrealProjects/AntiHero/BulletPhysics/Plugins/BulletNPP/Source/ThirdParty/BulletPhysicsEngineLibrary/lib/Win64/_build/Extras/Serialize/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("F:/UnrealProjects/AntiHero/BulletPhysics/Plugins/BulletNPP/Source/ThirdParty/BulletPhysicsEngineLibrary/lib/Win64/_build/Extras/ConvexDecomposition/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("F:/UnrealProjects/AntiHero/BulletPhysics/Plugins/BulletNPP/Source/ThirdParty/BulletPhysicsEngineLibrary/lib/Win64/_build/Extras/HACD/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("F:/UnrealProjects/AntiHero/BulletPhysics/Plugins/BulletNPP/Source/ThirdParty/BulletPhysicsEngineLibrary/lib/Win64/_build/Extras/GIMPACTUtils/cmake_install.cmake")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "F:/UnrealProjects/AntiHero/BulletPhysics/Plugins/BulletNPP/Source/ThirdParty/BulletPhysicsEngineLibrary/lib/Win64/_build/Extras/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
