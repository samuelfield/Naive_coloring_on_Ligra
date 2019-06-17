ifdef LONG
INTT = -DLONG
endif

ifdef EDGELONG
INTE = -DEDGELONG
endif

ifdef PD
PD = -DPD
endif

ifdef BYTE
CODE = -DBYTE
else ifdef NIBBLE
CODE = -DNIBBLE
else
CODE = -DBYTERLE
endif

ifdef LOWMEM
MEM = -DLOWMEM
endif


SRC_DIR = src
OBJ_DIR = obj

LDLIBS += -lcilkrts -fcilkplus
CPPFLAGS += -Iinclude -isystem ligra
CXXFLAGS += -Wall -std=c++14 -fcilkplus -lcilkrts -O3 -DCILK -lpthread $(INTT) $(INTE) $(CODE) $(PD) $(MEM)
# CXXFLAGS += -Wall -std=c++14 -fcilkplus -lcilkrts -g -DCILK -lpthread $(INTT) $(INTE) $(CODE) $(PD) $(MEM)



.PHONY: all clean

ALL: color_l.app color_n.app color_serial.app color_lf.app color_lfr.app

all: $(ALL)

color_l.app: $(SRC_DIR)/coloring_asynch_locks.cc
	$(CXX) -o color_l.app $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/coloring_asynch_locks.cc

color_lf.app: $(SRC_DIR)/coloring_asynch_lockfree.cc
	$(CXX) -o color_lf.app $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/coloring_asynch_lockfree.cc

color_lfr.app: $(SRC_DIR)/coloring_asynch_lockfreerecursive.cc
	$(CXX) -o color_lfr.app $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/coloring_asynch_lockfreerecursive.cc

color_n.app: $(SRC_DIR)/coloring_asynch_naive.cc
	$(CXX) -o color_n.app $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/coloring_asynch_naive.cc

color_serial.app: $(SRC_DIR)/coloring_serial_naive.cc
	$(CXX) -o color_serial.app $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/coloring_serial_naive.cc



clean: $(ALL)
	rm -f $(OBJ_DIR)/*.o color_l.app color_n.app color_serial.app color_lf.app color_lfr.app