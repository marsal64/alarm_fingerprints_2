################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../alarm_fingerprints_2.cpp \
../wavelet_ms.cpp 

OBJS += \
./alarm_fingerprints_2.o \
./wavelet_ms.o 

CPP_DEPS += \
./alarm_fingerprints_2.d \
./wavelet_ms.d 


# Each subdirectory must supply rules for building sources it contributes
alarm_fingerprints_2.o: ../alarm_fingerprints_2.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"alarm_fingerprints_2.d" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


