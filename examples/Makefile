CXX = g++
CFLAGS  = 
DEBUG_FLAGS = #-g
CXXFLAGS = -c
INCS = -I ../include -I ./ 

LIBS += -L ../lib64 -l tmfqsfs 

ifeq ($(USE_BLOSC), 1)
CFLAGS += -DUSE_BLOSC
LIBS += -lblosc2
endif

all: applyHadamard applyControlledPhaseShift applyControlledNot testQregQgate qft qftG grover getSumOfProbabilities


#LINK MAIN PROGRAMS
applyHadamard: applyHadamard.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) applyHadamard.o -o ../bin/applyHadamard

applyControlledPhaseShift: applyControlledPhaseShift.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) applyControlledPhaseShift.o -o ../bin/applyControlledPhaseShift

applyControlledNot: applyControlledNot.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) applyControlledNot.o -o ../bin/applyControlledNot

testQregQgate: testQregQgate.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) testQregQgate.o -o ../bin/testQregQgate

qft: qft.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) qft.o -o ../bin/qft

qftG: qftG.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) qftG.o -o ../bin/qftG

grover: grover.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) grover.o -o ../bin/grover

getSumOfProbabilities: getSumOfProbabilities.o
	$(CXX) $(LIBS) $(CFLAGS) $(DEBUG_FLAGS) getSumOfProbabilities.o -o ../bin/getSumOfProbabilities



#COMPILE SOURCE MAIN PROGRAMS
applyHadamard.o: applyHadamard.cpp
	$(CXX) $(INCS) $(CFLAGS) $(CXXFLAGS) $(DEBUG_FLAG) applyHadamard.cpp

applyControlledPhaseShift.o: applyControlledPhaseShift.cpp
	$(CXX) $(INCS) $(CFLAGS) $(CXXFLAGS) $(DEBUG_FLAG) applyControlledPhaseShift.cpp

applyControlledNot.o: applyControlledNot.cpp
	$(CXX) $(INCS) $(CFLAGS) $(CXXFLAGS) $(DEBUG_FLAG) applyControlledNot.cpp

testQregQgate.o: testQregQgate.cpp
	$(CXX) $(INCS) $(CFLAGS) $(CXXFLAGS) $(DEBUG_FLAG) testQregQgate.cpp

qft.o: qft.cpp
	$(CXX) $(INCS) $(CFLAGS) $(CXXFLAGS) $(DEBUG_FLAG) qft.cpp

qftG.o: qftG.cpp
	$(CXX) $(INCS) $(CFLAGS) $(CXXFLAGS) $(DEBUG_FLAG) qftG.cpp

grover.o: grover.cpp
	$(CXX) $(INCS) $(CFLAGS) $(CXXFLAGS) $(DEBUG_FLAG) grover.cpp

getSumOfProbabilities.o: getSumOfProbabilities.cpp
	$(CXX) $(INCS) $(CFLAGS) $(CXXFLAGS) $(DEBUG_FLAG) getSumOfProbabilities.cpp


#CLEAN BINARIES
clean: 
	rm -f *.o 
