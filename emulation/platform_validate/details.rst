Platform Validate Bare-Metal Emulation Test 
===========================================

The baremetal flow is used for emulation platform level basic testing i.e. to test different applications - DDR testing, LPDDR testing, AIE testing.  

The generate-platform.sh script ``Vitis/Install/Path/bin/generate-platform.sh`` reads the extensible XSA (exported from Vivado) and generates extensible platform (.xpfm) out of it. 

In this example, ``/proj/xbuilds/2022.1_daily_latest/internal_platforms/xilinx_vck190_base_202210_1/xilinx_vck190_base_202210_1.xpfm`` vck190 base extensible platform is used at v++ lin to generate the fixed XSA. For more details, please refer `Building the Bare-Metal System <https://docs.xilinx.com/r/en-US/ug1076-ai-engine-environment/Building-a-Bare-metal-System>`__

Input from the user
--------------------

1. The user needs to generate the fixed XSA from v++ link using extensible platform. 

Steps to run the Bare-Metal Emulation Test
------------------------------------------

1. Build the fixed xsa using v++ link step as per extensible platform:  
   ``make fixed_xsa PLATFORM=/proj/xbuilds/2022.1_daily_latest/internal_platforms/xilinx_vck190_base_202210_1/xilinx_vck190_base_202210_1.xpfm``

2. Build the BSP sources and libraries required for compilation of user application. Compile and link the user application to generate main.elf: ``make baremetal_elf``

* Note: Users can modify sw/main.cpp file and incrementally compile it to build the main.elf as per your user application.

3. Generate the package directory: ``make package``

4. Run the user application: ``./package.hw_emu/launch_hw_emu.sh``

* To run all the steps at once, use ``make run TARGET=hw_emu PLATFORM=/proj/xbuilds/2022.1_daily_latest/internal_platforms/xilinx_vck190_base_202210_1/xilinx_vck190_base_202210_1.xpfm``
