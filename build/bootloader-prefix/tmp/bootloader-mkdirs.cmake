# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/Espressif/frameworks/esp-idf-v5.1/components/bootloader/subproject"
  "E:/Liz/IMU/software/IMU_MCU/esp32c6_tcp_client_with_uart/build/bootloader"
  "E:/Liz/IMU/software/IMU_MCU/esp32c6_tcp_client_with_uart/build/bootloader-prefix"
  "E:/Liz/IMU/software/IMU_MCU/esp32c6_tcp_client_with_uart/build/bootloader-prefix/tmp"
  "E:/Liz/IMU/software/IMU_MCU/esp32c6_tcp_client_with_uart/build/bootloader-prefix/src/bootloader-stamp"
  "E:/Liz/IMU/software/IMU_MCU/esp32c6_tcp_client_with_uart/build/bootloader-prefix/src"
  "E:/Liz/IMU/software/IMU_MCU/esp32c6_tcp_client_with_uart/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/Liz/IMU/software/IMU_MCU/esp32c6_tcp_client_with_uart/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/Liz/IMU/software/IMU_MCU/esp32c6_tcp_client_with_uart/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
