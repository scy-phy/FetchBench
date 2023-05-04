PROJNAME=testccr
C=gcc
LNK=gcc
CFLAGS=-c -Wall -O0
LNKFLAGS=
BIN=bin
OBJ=obj

.PHONY: default clean
default: \
	$(BIN)/$(PROJNAME)
clean:
	rm -rf $(BIN) $(OBJ)

$(OBJ):
	@mkdir -p $(OBJ)
$(BIN):
	@mkdir -p $(BIN)

$(OBJ)/$(PROJNAME).o: $(PROJNAME).c | $(OBJ)
	$(C) -o $@ $(CFLAGS) $<

$(BIN)/$(PROJNAME): $(OBJ)/$(PROJNAME).o | $(BIN)
	$(LNK) -o $@ $(LNKFLAGS) $^