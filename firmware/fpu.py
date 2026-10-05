# PlatformIO extra script: compile and link everything for the Cortex-M4's
# hardware FPU.
#
# The FreeRTOS ARM_CM4F port saves FPU registers on a task switch, so it
# must be built with hard-float. Every object file and the C library must
# use the same float ABI or the linker refuses to combine them, which is
# why the flags go to the compiler, the assembler and the linker.
Import("env")

fpu_flags = ["-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"]

env.Append(
    CCFLAGS=fpu_flags,
    ASFLAGS=fpu_flags,
    ASPPFLAGS=fpu_flags,
    LINKFLAGS=fpu_flags,
)
