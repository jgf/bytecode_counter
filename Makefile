TARGET_LIB_NAME=counter
BCCOUNT_DIR=.
JAVAC=javac
JAVAC_FLAGS=-g -d .
JAVA_LD_LIB_PATH=${CURDIR}/${BCCOUNT_DIR}
UNAME=$(shell uname -s)
ifeq (${UNAME}, Darwin)
	TARGET_LIB=lib${TARGET_LIB_NAME}.jnilib
	JAVA_WITH_AGENT=DYLD_LIBRARY_PATH="${JAVA_LD_LIB_PATH}:$$DYLD_LIBRARY_PATH" java -agentlib:${TARGET_LIB_NAME}
	JAVA_HEADERS=/System/Library/Frameworks/JavaVM.framework/Headers
	CFLAGS_JNILIB=-fPIC -dynamiclib
else
	TARGET_LIB=lib${TARGET_LIB_NAME}.so
	JAVA_WITH_AGENT=LD_LIBRARY_PATH="${JAVA_LD_LIB_PATH}:$$LD_LIBRARY_PATH" java -agentlib:${TARGET_LIB_NAME}
	JAVA_HEADERS=/usr/lib/jvm/default-java/include
	CFLAGS_JNILIB=-fPIC -shared
endif
TARGET=${BCCOUNT_DIR}/${TARGET_LIB}
CFLAGS_OBJ=-I${BCCOUNT_DIR} -I${JAVA_HEADERS} -O0 -fPIC -Wall -g3 -c
OBJ=${BCCOUNT_DIR}/hashmap.o ${BCCOUNT_DIR}/count_instructions.o
HDR=${BCCOUNT_DIR}/hashmap.h
SRC=$(OBJ:%.o=%.c)
JAVA_TESTCLASS=Test
JAVA_AGENT_TEST_LOG=${JAVA_TESTCLASS}.agent.log
OBJ_TEST=${JAVA_TESTCLASS}.class

all: ${TARGET}

test: ${TARGET} ${OBJ_TEST}

run-test: test
	${JAVA_WITH_AGENT} ${JAVA_TESTCLASS} | tee ${JAVA_AGENT_TEST_LOG}

${TARGET}: ${OBJ}
	${CC} ${CFLAGS_JNILIB} -o $@ $^

%.o: %.c *.h
	${CC} ${CFLAGS_OBJ} -o $@ $<

%.class: %.java
	${JAVAC} ${JAVAC_FLAGS} $<

clean-all: clean
	${RM} ${JAVA_AGENT_TEST_LOG}
	${RM} ${TARGET}

clean:
	${RM} ${OBJ}
	${RM} ${OBJ_TEST}

