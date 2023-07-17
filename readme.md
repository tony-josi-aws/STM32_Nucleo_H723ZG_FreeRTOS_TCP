Minimal FreeRTOS+TCP demo project for STM32H7xx
-----------------------------------------------

The FreeRTOS-Kernel and FreeRTOS+TCP stack is submoduled into the `Libraries` folder.

Cloning and building the project
--------------------------------

* Clone the project: 
  * `git clone https://github.com/tony-josi-aws/STM32_Nucleo_H723ZG_FreeRTOS_TCP.git --recurse-submodules`
* Use `.project` file to open the project in STM32Cube IDE
* Use STM32Cube IDE UI to build/download/debug.

Getting Started
---------------

The NUCLEO board can be setup by connecting the board to the PC via USB PWR port of the board with a USB cable and a LAN cable connecting the  RJ45 connector of the board to any network.

The `USE_IPv6_END_POINTS` macro defined in the `Libraries\FreeRTOS-Plus-CLI\app_main.c` can be used to enable/disable the IPv6 endpoints. One IPv4 endpoint will always be enabled.

This demo project has two sub demos which can be enabled by setting the following configuration in the `Libraries\FreeRTOS-Plus-CLI\app_main.c`:
* `mainCREATE_TCP_ECHO_TASKS_SINGLE` - Create a TCP client tasks that connects to an echo server running on address `configTCP_ECHO_SERVER_ADDR` and port `echoTCP_ECHO_SERVER_PORT`, and attempts to send and receive the echoed back data.
* `mainCREATE_UDP_ECHO_TASKS_SINGLE` - Create a UDP client tasks that connects to an echo server running on address `configECHO_SERVER_ADDR_STRING` and port `configUDP_ECHO_SERVER_PORT`, and attempts to send and receive the echoed back data. `configECHO_SERVER_ADDR_STRING` can be either IPv4 or IPv6 address.

The demo prints out log messages through the USART3 interface, by default the USART3 communication between the target STM32 and the ST-LINK is enabled in the NUCLEO boards, and it should show up as Virtual COM port in the Ports section of the Device Manager in Windows PCs. The baud rate is set to `115200` bps.

