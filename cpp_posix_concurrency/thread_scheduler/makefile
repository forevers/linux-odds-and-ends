
SRCS = main.cpp 

PRODUCT := scheduler_demo

CC := g++

PTHREAD =-pthread
CFLAGS := 
CPPFLAGS := $(DEBUG) $(OPT) $(WARN) $(PTHREAD) -g -O0 -std=c++17
LDFLAGS := $(PTHREAD)
LD := g++

all: $(PRODUCT)

clean:
	$(RM) $(PRODUCT)

# build target
$(PRODUCT): main.cpp
	g++ -g -O0 -std=c++17 -pthread -ggdb -lpthread -o scheduler_demo main.cpp



