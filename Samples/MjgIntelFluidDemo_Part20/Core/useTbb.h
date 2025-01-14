/** \file useTbb.h

    \brief Common location to control whether to use Threading Building Blocks

    \see Accompanying articles for more information:
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-1/
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-18/

        - http://www.gamasutra.com/view/feature/4164/sponsored_feature_fluid_.php
        - http://www.gamasutra.com/view/feature/4176/sponsored_feature_fluid_.php

        - http://www.mijagourlay.com/

    \author Copyright 2009-2014 Dr. Michael Jason Gourlay; All rights reserved.  Contact me at mijagourlay.com for licensing.
*/
#ifndef USE_TBB_H
#define USE_TBB_H

/** Use Threading Building Blocks.

    If you want to use TBB, you must install the libraries.
    Obtain them from http://www.threadingbuildingblocks.org/.

    Place them in the appropriate directory.

    For MS Windows, you also need to set up the environment.
    TBB comes with a batch file to do that but you must run
    its contents from autoexec.bat, or else manually enter
    all of the same information via Start --> Control Panel
    --> System --> Advanced (tab) --> Environment Variables.

    In addition to those variables provided in the batch file,
    you will also likely need to set your PATH to include
    the "bin" directory associated with the library you choose,
    making sure that bit precedes other directories,
    otherwise you might get an error stating "cannot find entry
    point in dynamic link library tbb.dll", which implies your system
    has multiple copies of tbb.dll and the one you need comes
    after some different (probably older) version.  Alternatively you
    could put the tbb.dll in the same directory as the executable.

*/

//#if defined( USE_TBB )  // DO NOT SUBMIT.  Disabled temporarily to rule out whether non-determinism from multi-threading is the cause of an issue.
//#   undef USE_TBB       // DO NOT SUBMIT
//#   define USE_TBB 0    // DO NOT SUBMIT
//#endif                  // DO NOT SUBMIT

#if ! defined( USE_TBB )
#   if defined( WIN32 )
#       define USE_TBB 0
#   endif
#endif

//#undef USE_TBB
//#define USE_TBB 0

#if USE_TBB
#   pragma warning( disable: 4511 ) // TBB has issues: Copy constructor could not be generated
#   pragma warning( disable: 4512 ) // TBB has issues: Assignment operator could not be generated
#   if defined( _DEBUG )
#       pragma comment(lib, "tbb_debug.lib")
#   else
#       pragma comment(lib, "tbb.lib")
#   endif

#   include "tbb/task_scheduler_init.h"
#   include "tbb/parallel_for.h"
#   include "tbb/parallel_do.h"
#   include "tbb/parallel_reduce.h"
#   include "tbb/parallel_invoke.h"
#   include "tbb/blocked_range.h"
#   include "tbb/tick_count.h"
#   include "tbb/atomic.h"

    typedef tbb::atomic< bool > TbbAtomicBool ;

    // Simple scoped spin lock based in atomic bool, for synchronizing on any architecture with atomic instructions, even if the OS does not provide locks.
    class TbbScopedSpinLock
    {
    public:
        // Creating this object (e.g. on the stack as an automatic variable) obtains the lock.
        TbbScopedSpinLock( TbbAtomicBool & lock )
            : _lock( lock )
        {
            while( _lock.fetch_and_store( true ) ) { } // Obtain lock.
        }

        // Destructing this object (e.g. leaving scope) releases the lock.
        ~TbbScopedSpinLock()
        {
            _lock = false ; // Release lock.
        }

    private:
        TbbScopedSpinLock() ;                                       // Disallow default construction
        TbbScopedSpinLock( const TbbScopedSpinLock & ) ;            // Disallow copy
        TbbScopedSpinLock operator=( const TbbScopedSpinLock & ) ;  // Disallow assignment

        TbbAtomicBool & _lock ;
    } ;

#   define TBB_SCOPED_SPIN_LOCK( lock ) TbbScopedSpinLock tbbScopedSpinLock ## lock( lock )

#else

    //typedef bool TbbAtomicBool ;

#   define TBB_SCOPED_SPIN_LOCK( lock )

#endif

extern unsigned gNumberOfProcessors ;  ///< Number of processors this machine has.

#if USE_TBB
static inline unsigned GetNumberOfProcessors()
{
    unsigned numberOfProcessors = 0 ;
    // Query environment for number of processors on this machine.
    const char * strNumberOfProcessors = getenv( "NUMBER_OF_PROCESSORS" ) ;
    if( strNumberOfProcessors != 0 )
    {   // Environment contains a value for number of processors.
        numberOfProcessors = atoi( strNumberOfProcessors ) ;
    }
    return numberOfProcessors ;
}
#endif

#endif
