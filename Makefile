CC = gcc 	# C compiler

JVMTI_PATH=/usr/lib/jvm/java-1.8.0-openjdk-1.8.0.121-1.b14.fc25.x86_64/include              
JVMTI_PATH_LINUX=/usr/lib/jvm/java-1.8.0-openjdk-1.8.0.121-1.b14.fc25.x86_64/include/linux   

WNO= -Wno-sign-compare -Wno-discarded-qualifiers -Wno-unused-parameter
CFLAGS = -I${JVMTI_PATH} -I${JVMTI_PATH_LINUX} -fPIC -Wall -Wextra -O0 -g3 -fno-omit-frame-pointer 
CFLAGS+=${WNO}

LDFLAGS = -L${JVMTI_PATH} -L${JVMTI_PATH_LINUX} -shared  		

RM = rm -f  		
TARGET_LIB = libnat_agent

SRCS = src/main.c src/agent.c src/basic.c src/byte.c src/class.c src/dump.c src/par.c    
OBJS = $(SRCS:.c=.o)

.PHONY: all
all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o output/$@ $^ 
		
$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS)  -MM $< >$@                         

include $(SRCS:.c=.d)

.PHONY: clean
clean:
	${RM} output/${TARGET_LIB} ${OBJS} $(SRCS:.c=.d)
