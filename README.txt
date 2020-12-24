Interrupt Latency and context switch measurement on Zephyr RTOS for Galileo Gen2 

The application will measure:

Interrupt latency with no background task, only a single task is running to trigger the GPIO pin for the interrupt. 

Interrupt latency with a background task - a simple message passing between two threads which might delay the interrupt handler because of the use of the data passing message queue API.

Context switch latency using synchronization methods. 

For Zephyr Version 1.14.0 
Doc: https://docs.zephyrproject.org/1.14.0/
Zephyr 1.14.0 source code: https://github.com/zephyrproject-rtos/zephyr/tree/v1.14-branch

What you will also need is a galileo gen 2 board, an RGD LED module and some male/female connecting wires 

Please follow the following procedure to run the application:

Step 1: Find the files CMakeLists.txt, prj.conf and the folder src with the file main.c

Step 2: Copy these file to a folder (add any "file_name") in zephyr/samples 

Step 3: Open terminal to set environment variables as following:
        
        "export $ZEPHYR_TOOLCHAIN_VARIANT=zephyr"
        "export $ZEPHYR_SDK_INSTALL_DIR= ~/path_to_sdk_install_dir"
         cd to zephyr in terminal
        "source zephyr-env.sh" 

Step 4: In order to build the application use the following steps:
         
        cd to zephyr/samples/"file_name"
        mkdir build && cd build
        cmake -DBOARD=galileo ..
        make 
 
Step 5: Copy file zephyr.strip from "file_name"/build/zephyr to the SD CARD.

Step 6: Connect the IO pins on the galieo as following:
        
        IO10 to be connected to IO3 for GPIO interrupt.
        IO9 to be connected to IO2 for PWM interrupt.
        Connect rgb module ground.
        IO1 to be connected to RED of rgb module.
        IO12 to be connected to GREEN of rgb module.

Step 7: Place SD card in galileo and boot the board.

Step 8: Shell will start to run. You can use the following commands for the measurements:  
        Type "measure" and press enter.
        You will see the following commands:
        measure 1: Interrupt latency measurement w/o background process.
        measure 2: Interrupt latency measurement w/background process.
        measure 3: Context switch latency measurement.

Step 9: Run the measurement with any of the above commands.
----------------------------------------------------------------------------------------------------------------------------------------------
Note: The following is the expected execution of the application.

The application will print which measurement is being taken (when run) and which measurement has been completed (when finished).

Green led remains on when the system is free/ready to measure. When a measure command is run the led turns red while it is busy. Once the measurements are complete the led turns green again afer printing out the results of the measurements.

In measurement 2 the application will print when the background threads have started. 


 
