################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../os345.c \
../os345fat.c \
../os345lc3.c \
../os345mmu.c \
../os345p1.c \
../os345p2.c \
../os345p3.c \
../os345p4.c \
../os345p5.c \
../os345p6.c \
../os345park.c 

OBJS += \
./os345.o \
./os345fat.o \
./os345lc3.o \
./os345mmu.o \
./os345p1.o \
./os345p2.o \
./os345p3.o \
./os345p4.o \
./os345p5.o \
./os345p6.o \
./os345park.o 

C_DEPS += \
./os345.d \
./os345fat.d \
./os345lc3.d \
./os345mmu.d \
./os345p1.d \
./os345p2.d \
./os345p3.d \
./os345p4.d \
./os345p5.d \
./os345p6.d \
./os345park.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


