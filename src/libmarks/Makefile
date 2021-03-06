# moss
.DEFAULT: all
.PHONY: all clean install

# Disable warnings from boost files
BOOSTISMS = -Wno-c++11-extra-semi -Wno-c++11-long-long
CC = clang++
CFLAGS = -Wall -Werror -pedantic $(BOOSTISMS)

# location of the Python header files
PYTHON_VERSION = 3.6
PYTHON_INCLUDE = /usr/include/python3.6m
PYTHON_LIBNAME = python3.6m
PYTHON_LIB = /usr/lib/python$(PYTHON_VERSION)

PYTHON_INCLUDES = -I$(PYTHON_INCLUDE)
PYTHON_LDFLAGS = -L$(PYTHON_LIB)/config -l$(PYTHON_LIBNAME)

# location of the Boost Python include files and library
BOOST_INC = /local/courses/csse2310/boost/current/include
BOOST_LIB = /local/courses/csse2310/boost/current/lib
BOOST_LIBNAME = boost_python36

PROCESS_PARTIALS = src/glue.o src/process.o src/tracedProcess.o
TARGETS = $(PROCESS_PARTIALS) src/process.so src/libprotect.so

DESTINATION = ../marks

all: $(TARGETS) src/process.so
	@# Create destination directory
	mkdir -p $(DESTINATION)
	@# Copy the compiled library
	cp src/process.so $(DESTINATION)/process.so
	cp src/libprotect.so $(DESTINATION)/libprotect.so
	@# Compile and copy the Python module
	python3 compile_py.py py_src $(DESTINATION)

debug: CFLAGS += -g -DDEBUG
debug: clean all

clean:
	rm -f $(TARGETS)
	rm -rf $(DESTINATION)

src/process.so: $(PROCESS_PARTIALS)
	$(CC) -shared -Wl,--export-dynamic $(PROCESS_PARTIALS) -L$(BOOST_LIB) -l$(BOOST_LIBNAME) $(PYTHON_LDFLAGS) -o $@

src/glue.o: src/glue.cpp src/process.hpp
	$(CC) $(PYTHON_INCLUDES) -I$(BOOST_INC) -fPIC -pthread -c $< -o $@

src/process.o: src/process.cpp src/process.hpp
	$(CC) $(CFLAGS) $(PYTHON_INCLUDES) -I$(BOOST_INC) -fPIC -pthread -c $< -o $@

src/tracedProcess.o: src/traced_process.cpp src/process.hpp
	$(CC) $(CFLAGS) $(PYTHON_INCLUDES) -I$(BOOST_INC) -fPIC -pthread -c $< -o $@

src/libprotect.so: src/protection.c
	gcc -std=gnu99 $(CFLAGS) -ldl -shared -Wl,--export-dynamic -fPIC $< -o $@

install:	all
	install -m u=rw,g=r,o=r ../marks/*.pyc /local/courses/csse2310/libtest/marks
	install -m u=rw,g=r,o=r ../marks/*.so /local/courses/csse2310/libtest/marks
