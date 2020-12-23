##############################################################################################################################################
GPIO and Interrupt Latency Measurement  ---------------------------------------------------------------------------------------------------------------------------------------------- 
NAME: ARJUN BHATNAGAR            ----------------------------------------------------------------------------------------------------------------------------------------------

Please follow the following procedure to run the application:

Step 1: In the folder measure_43,you will find the files CMakeLists.txt
        and prj.conf and the folder src with the file main.c
Step 2: Copy this folder to zephyr/samples 
Step 3: Open terminal set environment variables as following:
        
        "export $ZEPHYR_TOOLCHAIN_VARIANT=zephyr"
        "export $ZEPHYR_SDK_INSTALL_DIR= ~/path_to_sdk_install_dir"
         cd to zephyr in terminal
        "source zephyr-env.sh" 

Step 4: In order to build the application use the following steps:
         
        cd to zephyr/samples/measure_43
        mkdir build && cd build
        cmake -DBOARD=galileo ..
        make 
 
Step 5: copy file zephyr.strip from measure/build/zephyr to the SD CARD.

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


 
