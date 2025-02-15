/*! \file useTbb.h

    \brief Common location to control whether to use Threading Building Blocks

    \see Accompanying articles for more information:
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-1/
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-2/
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-3/
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-4/
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-5/
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-6/
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-7/
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-8/
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-9/
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-10/
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-11/
        - http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-12/

        - http://www.gamasutra.com/view/feature/4164/sponsored_feature_fluid_.php
        - http://www.gamasutra.com/view/feature/4176/sponsored_feature_fluid_.php

        - http://www.mijagourlay.com/

    \author Copyright 2009-2012 Dr. Michael Jason Gourlay; All rights reserved.  Contact me at mijagourlay.com for licensing.
*/
#ifndef USE_TBB_H
#define USE_TBB_H

/*! \brief Use Threading Building Blocks

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
#if ! defined( USE_TBB )
    #if defined( WIN32 )
        #define USE_TBB 1
    #endif
#endif

#if USE_TBB
    #pragma warning( disable: 4511 ) // TBB has issues: Copy constructor could not be generated
    #pragma warning( disable: 4512 ) // TBB has issues: Assignment operator could not be generated

    #include "tbb/task_scheduler_init.h"
    #include "tbb/parallel_for.h"
    #include "tbb/parallel_reduce.h"
    #include "tbb/parallel_invoke.h"
    #include "tbb/blocked_range.h"
    #include "tbb/tick_count.h"
#endif

extern unsigned gNumberOfProcessors ;  ///< Number of processors this machine has.

#endif
