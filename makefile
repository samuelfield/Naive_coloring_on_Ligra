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
BIN_DIR = bin

LDLIBS += -lcilkrts -fcilkplus
CPPFLAGS += -Iinclude -isystem ligra
CXXFLAGS += -Wall -std=c++14 -fcilkplus -lcilkrts -O3 -DCILK -lpthread $(INTT) $(INTE) $(CODE) $(PD) $(MEM)
# CXXFLAGS += -Wall -std=c++14 -fcilkplus -lcilkrts -g -DCILK -lpthread $(INTT) $(INTE) $(CODE) $(PD) $(MEM)

.PHONY: all clean

ALL: asynch_locks asynch_lockfree asynch_naive asynch_push_passive asynch_push_active serial_naive asynch_verification

all: $(ALL)

asynch_locks: $(SRC_DIR)/asynch_locks.cc
	$(CXX) -o $(BIN_DIR)/asynch_locks $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/asynch_locks.cc

asynch_lockfree: $(SRC_DIR)/asynch_lockfree.cc
	$(CXX) -o $(BIN_DIR)/asynch_lockfree $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/asynch_lockfree.cc

asynch_naive: $(SRC_DIR)/asynch_naive.cc
	$(CXX) -o $(BIN_DIR)/asynch_naive $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/asynch_naive.cc

asynch_push_passive: $(SRC_DIR)/asynch_push_passive.cc
	$(CXX) -o $(BIN_DIR)/asynch_push_passive $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/asynch_push_passive.cc

asynch_push_active: $(SRC_DIR)/asynch_push_active.cc
	$(CXX) -o $(BIN_DIR)/asynch_push_active $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/asynch_push_active.cc

asynch_verification: $(SRC_DIR)/asynch_verification.cc
	$(CXX) -o $(BIN_DIR)/asynch_verification $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/asynch_verification.cc

serial_naive: $(SRC_DIR)/serial_naive.cc
	$(CXX) -o $(BIN_DIR)/serial_naive $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/serial_naive.cc

# color_cm.app: $(SRC_DIR)/coloring_asynch_locksCM.cc
# 	$(CXX) -o color_cm.app $(CPPFLAGS) $(CXXFLAGS) $(SRC_DIR)/coloring_asynch_locksCM.cc

clean: $(ALL)
	rm -f $(OBJ_DIR)/*.o $(BIN_DIR)/*