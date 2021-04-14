CC := gcc
CCP := g++
BIN := edd_scheduler

BUILD_DIR := build

FREERTOS_DIR_REL := ./FreeRTOS/FreeRTOS
FREERTOS_DIR := $(abspath $(FREERTOS_DIR_REL))

#FREERTOS_PLUS_DIR_REL := ../../../FreeRTOS-Plus
#FREERTOS_PLUS_DIR := $(abspath $(FREERTOS_PLUS_DIR_REL))

INCLUDE_DIRS := -I.
INCLUDE_DIRS += -I${FREERTOS_DIR}/Source/include
INCLUDE_DIRS += -I${FREERTOS_DIR}/Source/portable/ThirdParty/GCC/Posix
INCLUDE_DIRS += -I${FREERTOS_DIR}/Source/portable/ThirdParty/GCC/Posix/utils
#INCLUDE_DIRS += -I${FREERTOS_PLUS_DIR}/Source/FreeRTOS-Plus-Trace/Include

SOURCE_FILES := $(wildcard *.c)
SOURCE_FILES += $(wildcard *.cpp)
SOURCE_FILES += $(wildcard ${FREERTOS_DIR}/Source/*.c)
# Memory manager (use malloc() / free() )
SOURCE_FILES += ${FREERTOS_DIR}/Source/portable/MemMang/heap_3.c
# posix port
SOURCE_FILES += ${FREERTOS_DIR}/Source/portable/ThirdParty/GCC/Posix/utils/wait_for_event.c
SOURCE_FILES += ${FREERTOS_DIR}/Source/portable/ThirdParty/GCC/Posix/port.c

# Trace library.
#SOURCE_FILES += ${FREERTOS_PLUS_DIR}/Source/FreeRTOS-Plus-Trace/trcKernelPort.c
#SOURCE_FILES += ${FREERTOS_PLUS_DIR}/Source/FreeRTOS-Plus-Trace/trcSnapshotRecorder.c
#SOURCE_FILES += ${FREERTOS_PLUS_DIR}/Source/FreeRTOS-Plus-Trace/trcStreamingRecorder.c
#SOURCE_FILES += ${FREERTOS_PLUS_DIR}/Source/FreeRTOS-Plus-Trace/streamports/File/trcStreamingPort.c

# EDD and sample application
#TODO: add more files as we make them. anything in root should be added already

CFLAGS := -ggdb3 -O0 -DprojCOVERAGE_TEST=0 -D_WINDOWS_
LDFLAGS := -ggdb3 -O0 -pthread

C_OBJ_FILES = $(SOURCE_FILES:%.c=$(BUILD_DIR)/%.o)
CPP_OBJ_FILES = $(SOURCE_FILES:%.cpp=$(BUILD_DIR)/%.o)

C_DEP_FILE = $(C_OBJ_FILES:%.o=%.d)
#CPP_DEP_FILE = $(CPP_OBJ_FILES:%.o=%.d) #this line is problem?

${BIN} : $(BUILD_DIR)/$(BIN)

${BUILD_DIR}/${BIN} : ${C_OBJ_FILES}# ${CPP_OBJ_FILES}
	-mkdir -p ${@D}
	$(CC) $^ $(CFLAGS) $(INCLUDE_DIRS) ${LDFLAGS} -o $@

# ${BUILD_DIR}/${BIN} : ${CPP_OBJ_FILES}
# 	-mkdir -p ${@D}
# 	$(CCP) $^ $(CFLAGS) $(INCLUDE_DIRS) ${LDFLAGS} -o $@


-include ${C_DEP_FILE} #${CPP_DEP_FILE}

${BUILD_DIR}/%.o : %.c
	-mkdir -p $(@D)
	$(CC) $(CFLAGS) ${INCLUDE_DIRS} -MMD -c $< -o $@

# ${BUILD_DIR}/%.o : %.cpp
# 	-mkdir -p $(@D)
# 	$(CCP) $(CFLAGS) ${INCLUDE_DIRS} -MMD -c $< -o $@

.PHONY: clean

clean:
	-rm -rf $(BUILD_DIR)







