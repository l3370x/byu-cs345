################################################################################
# Automatically-generated file. Do not edit!
################################################################################

RM := rm -rf

# All of the sources participating in the build are defined here
# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
os345.c \
os345fat.c \
os345lc3.c \
os345mmu.c \
os345p1.c \
os345p2.c \
os345p3.c \
os345p4.c \
os345p5.c \
os345p6.c \
os345park.c 

OBJS += \
obj/os345.o \
obj/os345fat.o \
obj/os345lc3.o \
obj/os345mmu.o \
obj/os345p1.o \
obj/os345p2.o \
obj/os345p3.o \
obj/os345p4.o \
obj/os345p5.o \
obj/os345p6.o \
obj/os345park.o 

C_DEPS += \
obj/os345.d \
obj/os345fat.d \
obj/os345lc3.d \
obj/os345mmu.d \
obj/os345p1.d \
obj/os345p2.d \
obj/os345p3.d \
obj/os345p4.d \
obj/os345p5.d \
obj/os345p6.d \
obj/os345park.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: %.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "obj/$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif


# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: 345_2

# Tool invocations
345_2: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc  -o "345_2" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) 345_2
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
