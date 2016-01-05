################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
Images/LPRocket_96x37.obj: ../Images/LPRocket_96x37.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: MSP430 Compiler'
	"c:/ti/ccsv6/tools/compiler/msp430_4.3.3/bin/cl430" -vmspx --abi=eabi --data_model=restricted -O3 --include_path="c:/ti/ccsv6/ccs_base/msp430/include" --include_path="c:/ti/ccsv6/tools/compiler/msp430_4.3.3/include" --include_path="E:/Codes/CCS/workspace_v6_2/blinkSpeech/driverlib/MSP430F5xx_6xx" --include_path="E:/Codes/CCS/workspace_v6_2/blinkSpeech" --include_path="E:/Codes/CCS/workspace_v6_2/blinkSpeech/grlib" --include_path="E:/Codes/CCS/workspace_v6_2/blinkSpeech/fonts" --advice:power="none" -g --define=__MSP430F5529__ --diag_warning=225 --display_error_number --diag_wrap=off --silicon_errata=CPU21 --silicon_errata=CPU22 --silicon_errata=CPU23 --silicon_errata=CPU40 --large_memory_model --printf_support=minimal --preproc_with_compile --preproc_dependency="Images/LPRocket_96x37.pp" --obj_directory="Images" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

Images/TI_Logo_69x64.obj: ../Images/TI_Logo_69x64.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: MSP430 Compiler'
	"c:/ti/ccsv6/tools/compiler/msp430_4.3.3/bin/cl430" -vmspx --abi=eabi --data_model=restricted -O3 --include_path="c:/ti/ccsv6/ccs_base/msp430/include" --include_path="c:/ti/ccsv6/tools/compiler/msp430_4.3.3/include" --include_path="E:/Codes/CCS/workspace_v6_2/blinkSpeech/driverlib/MSP430F5xx_6xx" --include_path="E:/Codes/CCS/workspace_v6_2/blinkSpeech" --include_path="E:/Codes/CCS/workspace_v6_2/blinkSpeech/grlib" --include_path="E:/Codes/CCS/workspace_v6_2/blinkSpeech/fonts" --advice:power="none" -g --define=__MSP430F5529__ --diag_warning=225 --display_error_number --diag_wrap=off --silicon_errata=CPU21 --silicon_errata=CPU22 --silicon_errata=CPU23 --silicon_errata=CPU40 --large_memory_model --printf_support=minimal --preproc_with_compile --preproc_dependency="Images/TI_Logo_69x64.pp" --obj_directory="Images" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '


