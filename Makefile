CC=g++ -g -Wall -std=c++17 -Wno-deprecated-declarations


UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
    CC+=-D_XOPEN_SOURCE
    LIBFSCLIENT=libfs_client_macos.o
    LIBFSSERVER=libfs_server_macos.o
    BOOST_THREAD=boost_thread-mt
else
    LIBFSCLIENT=libfs_client.o
    LIBFSSERVER=libfs_server.o
    BOOST_THREAD=boost_thread
endif

# List of source files for your file server
FS_SOURCES=fs_server.cpp

# Generate the names of the file server's object files
FS_OBJS=${FS_SOURCES:.cpp=.o}

all: fs test1 test2 test3 test4

# Compile the file server and tag this compilation
#
# Remember to set the CPLUS_INCLUDE_PATH, LIBRARY_PATH, and LD_LIBRARY_PATH
# environment variables to include your Boost installation directory.
fs: ${FS_OBJS} ${LIBFSSERVER}
	./autotag.sh push
	${CC} -o $@ $^ -l${BOOST_THREAD} -lboost_system -pthread -ldl -lboost_regex

# Compile a client program
test1: test1.cpp ${LIBFSCLIENT}
	${CC} -o $@ $^

test2: testyiyao4.cpp ${LIBFSCLIENT}
	${CC} -o $@ $^

test3: testyiyao3.cpp ${LIBFSCLIENT}
	${CC} -o $@ $^

test4: sendreq.cpp ${LIBFSCLIENT}
	${CC} -o $@ $^

# Generic rules for compiling a source file to an object file
%.o: %.cpp
	${CC} -c $<
%.o: %.cc
	${CC} -c $<

clean:
	rm -f ${FS_OBJS} fs test1 test2 test3 test4
