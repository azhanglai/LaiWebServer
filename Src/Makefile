CXX := g++
CFLAGS := -std=c++14 -O1 -g 
INC := ./include/

OBJ_DIR := ./obj
BIN_DIR := ./bin

OBJS = ${OBJ_DIR}/main.o ${OBJ_DIR}/webserver.o ${OBJ_DIR}/epoller.o       \
	   ${OBJ_DIR}/sqlconnpool.o ${OBJ_DIR}/buffer.o ${OBJ_DIR}/heaptimer.o \
	   ${OBJ_DIR}/httprequest.o ${OBJ_DIR}/httpresponse.o ${OBJ_DIR}/httpconn.o \
	   ${OBJ_DIR}/config.o

.PHONY: mk_dir bin clean

all: mk_dir bin

mk_dir:
	if [ ! -d ${OBJ_DIR}  ]; then mkdir ${OBJ_DIR};fi
	if [ ! -d ${BIN_DIR}  ]; then mkdir ${BIN_DIR};fi

bin: $(OBJS)
	${CXX} ${CFLAGS} ${OBJS} -o ./bin/server -pthread -lmysqlclient 

${OBJ_DIR}/main.o: main.cpp
	${CXX} ${CFLAGS} -I ${INC} -o $@ -c $<

${OBJ_DIR}/webserver.o: ./webserver/webserver.cpp
	${CXX} ${CFLAGS} -I ${INC} -o $@ -c $<

${OBJ_DIR}/epoller.o: ./epoller/epoller.cpp 
	${CXX} ${CFLAGS} -I ${INC} -o $@ -c $<

${OBJ_DIR}/sqlconnpool.o: ./pool/sqlconnpool.cpp 
	${CXX} ${CFLAGS} -I ${INC} -o $@ -c $<

${OBJ_DIR}/buffer.o: ./buffer/buffer.cpp 
	${CXX} ${CFLAGS} -I ${INC} -o $@ -c $<

${OBJ_DIR}/heaptimer.o: ./timer/heaptimer.cpp 
	${CXX} ${CFLAGS} -I ${INC} -o $@ -c $<

${OBJ_DIR}/httprequest.o: ./http/httprequest.cpp 
	${CXX} ${CFLAGS} -I ${INC} -o $@ -c $<

${OBJ_DIR}/httpresponse.o: ./http/httpresponse.cpp 
	${CXX} ${CFLAGS} -I ${INC} -o $@ -c $<

${OBJ_DIR}/httpconn.o: ./http/httpconn.cpp 
	${CXX} ${CFLAGS} -I ${INC} -o $@ -c $<

${OBJ_DIR}/config.o: ./config/config.cpp
	${CXX} ${CFLAGS} -I ${INC} -o $@ -c $<

clean:
	rm -rf ./bin ./obj 

