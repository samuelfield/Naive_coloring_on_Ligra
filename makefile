EXE = color.app

SRC_DIR = src
OBJ_DIR = obj

SRC = $(wildcard $(SRC_DIR)/*.cc)
OBJ = $(SRC:$(SRC_DIR)/%.cc=$(OBJ_DIR)/%.o)

LDLIBS += -lcilkrts
CPPFLAGS += -Iinclude -isystem ligra
CXXFLAGS += -Wall -fcilkplus -g -lcilkrts

.PHONY: all clean

all: $(EXE)

$(EXE): $(OBJ)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ)