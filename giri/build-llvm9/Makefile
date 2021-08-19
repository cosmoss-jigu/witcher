CXX=g++
CXXINC= -I../include -I$(LLVM9_HOME)/include -I$(LLVM9_HOME)/build/include
#CXXFLAGS= -g -c -fPIC -std=c++17 $(CXXINC) -DDEBUG_GIRI_RUNTIME
CXXFLAGS= -g -c -fPIC -std=c++17 $(CXXINC)
#CXXLD= -L$(LLVM9_HOME)/build/lib -lLLVMSupport -ltinfo -lpthread -lstdc++fs
CXXLD= -L$(LLVM9_HOME)/build/lib -lLLVMSupport -lpthread -lstdc++fs

GIRI=../lib/Giri
UTILITY=../lib/Utility
WITCHER=../lib/Witcher
RUNTIME=../runtime/Giri
TOOLS=../tools

all: libgiri.so libdgutility.so libwitcher.so librtgiri.a tools

### libgiri
libgiri.so: Giri.o TracingNoGiri.o TraceFile.o
	$(CXX) Giri.o TracingNoGiri.o TraceFile.o -shared -o libgiri.so

Giri.o: $(GIRI)/Giri.cpp
	$(CXX) $(CXXFLAGS) $(GIRI)/Giri.cpp -o Giri.o
TracingNoGiri.o: $(GIRI)/TracingNoGiri.cpp
	$(CXX) $(CXXFLAGS) $(GIRI)/TracingNoGiri.cpp -o TracingNoGiri.o
TraceFile.o: $(GIRI)/TraceFile.cpp
	$(CXX) $(CXXFLAGS) $(GIRI)/TraceFile.cpp -o TraceFile.o

### libutility
libdgutility.so: BasicBlockNumbering.o CountSrcLines.o LoadStoreNumbering.o PostDominatorFrontier.o SourceLineMapping.o
	$(CXX) BasicBlockNumbering.o CountSrcLines.o LoadStoreNumbering.o PostDominatorFrontier.o SourceLineMapping.o -shared -o libdgutility.so

BasicBlockNumbering.o: $(UTILITY)/BasicBlockNumbering.cpp
	$(CXX) $(CXXFLAGS) $(UTILITY)/BasicBlockNumbering.cpp -o BasicBlockNumbering.o
CountSrcLines.o: $(UTILITY)/CountSrcLines.cpp
	$(CXX) $(CXXFLAGS) $(UTILITY)/CountSrcLines.cpp -o CountSrcLines.o
LoadStoreNumbering.o: $(UTILITY)/LoadStoreNumbering.cpp
	$(CXX) $(CXXFLAGS) $(UTILITY)/LoadStoreNumbering.cpp -o LoadStoreNumbering.o
PostDominatorFrontier.o: $(UTILITY)/PostDominatorFrontier.cpp
	$(CXX) $(CXXFLAGS) $(UTILITY)/PostDominatorFrontier.cpp -o PostDominatorFrontier.o
SourceLineMapping.o: $(UTILITY)/SourceLineMapping.cpp
	$(CXX) $(CXXFLAGS) $(UTILITY)/SourceLineMapping.cpp -o SourceLineMapping.o

### libwitcher
libwitcher.so: ProgramDependenceGraph.o WitcherPDG.o WitcherPMTrace.o WitcherPPDG.o WitcherParallelPDG.o WitcherParallelPPDG.o
	$(CXX) ProgramDependenceGraph.o WitcherPDG.o WitcherPMTrace.o WitcherPPDG.o WitcherParallelPDG.o WitcherParallelPPDG.o -shared -o libwitcher.so

ProgramDependenceGraph.o: $(WITCHER)/ProgramDependenceGraph.cpp
	$(CXX) $(CXXFLAGS) $(WITCHER)/ProgramDependenceGraph.cpp -o ProgramDependenceGraph.o
WitcherPDG.o: $(WITCHER)/WitcherPDG.cpp
	$(CXX) $(CXXFLAGS) $(WITCHER)/WitcherPDG.cpp -o WitcherPDG.o
WitcherPMTrace.o: $(WITCHER)/WitcherPMTrace.cpp
	$(CXX) $(CXXFLAGS) $(WITCHER)/WitcherPMTrace.cpp -o WitcherPMTrace.o
WitcherPPDG.o: $(WITCHER)/WitcherPPDG.cpp
	$(CXX) $(CXXFLAGS) $(WITCHER)/WitcherPPDG.cpp -o WitcherPPDG.o
WitcherParallelPDG.o: $(WITCHER)/WitcherParallelPDG.cpp
	$(CXX) $(CXXFLAGS) $(WITCHER)/WitcherParallelPDG.cpp -o WitcherParallelPDG.o
WitcherParallelPPDG.o: $(WITCHER)/WitcherParallelPPDG.cpp
	$(CXX) $(CXXFLAGS) $(WITCHER)/WitcherParallelPPDG.cpp -o WitcherParallelPPDG.o

### librtgiri
librtgiri.a: Tracing.o
	ar cru librtgiri.a Tracing.o
	ranlib librtgiri.a

Tracing.o: $(RUNTIME)/Tracing.cpp
	$(CXX) $(CXXFLAGS) $(RUNTIME)/Tracing.cpp -o Tracing.o

### tools
tools: prtrace pmtrace tracesplit tracesplitmt tracesplitbb

prtrace: PrintTrace.o
	$(CXX) PrintTrace.o -o prtrace $(CXXLD)
PrintTrace.o: $(TOOLS)/PrintTrace/PrintTrace.cpp
	$(CXX) $(CXXFLAGS) $(TOOLS)/PrintTrace/PrintTrace.cpp -o PrintTrace.o

pmtrace: PrintPMTrace.o
	$(CXX) PrintPMTrace.o -o pmtrace $(CXXLD)
PrintPMTrace.o: $(TOOLS)/PrintPMTrace/PrintPMTrace.cpp
	$(CXX) $(CXXFLAGS) $(TOOLS)/PrintPMTrace/PrintPMTrace.cpp -o PrintPMTrace.o

tracesplit: TraceSplit.o
	$(CXX) TraceSplit.o -o tracesplit $(CXXLD)
TraceSplit.o: $(TOOLS)/TraceSplit/TraceSplit.cpp
	$(CXX) $(CXXFLAGS) $(TOOLS)/TraceSplit/TraceSplit.cpp -o TraceSplit.o

tracesplitmt: TraceSplitMT.o
	$(CXX) TraceSplitMT.o -o tracesplitmt $(CXXLD)
TraceSplitMT.o: $(TOOLS)/TraceSplitMT/TraceSplitMT.cpp
	$(CXX) $(CXXFLAGS) $(TOOLS)/TraceSplitMT/TraceSplitMT.cpp -o TraceSplitMT.o

tracesplitbb: TraceSplitBB.o
	$(CXX) TraceSplitBB.o -o tracesplitbb $(CXXLD)
TraceSplitBB.o: $(TOOLS)/TraceSplitBB/TraceSplitBB.cpp
	$(CXX) $(CXXFLAGS) $(TOOLS)/TraceSplitBB/TraceSplitBB.cpp -o TraceSplitBB.o

### misc
clean:
	rm -f *.o *.so *.a prtrace pmtrace tracesplit*
