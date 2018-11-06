#include "boost/lexical_cast.hpp"

#include "flow/Trace.h"
#include "flow/DeterministicRandom.h"
#include "flow/SignalSafeUnwind.h"

#include "flow/flow.h"
#include "flow/TDMetric.actor.h"
#include "flow/SimpleOpt.h"

#include <signal.h>

#ifdef __unixish__
#include <stdio.h>
#endif

#ifndef WIN32
#include "versions.h"
#endif

#include "flow/actorcompiler.h"  // This must be the last #include.

int main(int argc, char **argv) {
	platformInit();
	initSignalSafeUnwind();
	Error::init();
	std::set_new_handler( &platform::outOfMemory );
	uint64_t memLimit = 8LL << 30;
	setMemoryQuota( memLimit );

	registerCrashHandler();

	try {
		g_random = new DeterministicRandom( platform::getRandomSeed() );
		g_network = newNet2(NetworkAddress(), false);

		Promise<int> pTest;
		Future<int> fTest = pTest.getFuture();
		pTest.send( 4 );
		printf( "%d\n", fTest.get() ); // fTest is already set

		g_network->stop();

		if(fTest.isReady()) {
			return fTest.get();
		}
		else {
			return 1;
		}
	} catch (Error& e) {
			printf("ERROR: %s (%d)\n", e.what(), e.code());
			return 1;
		}
}
