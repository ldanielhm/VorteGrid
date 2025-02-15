/*! \file PclOpKillAge.h

    \brief Particle operation to kill particles based on their age

    \see Accompanying articles for more information:
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-1/
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-2/
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-3/
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-4/
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-5/
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-6/
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-7/

        http://www.gamasutra.com/view/feature/4164/sponsored_feature_fluid_.php
        http://www.gamasutra.com/view/feature/4176/sponsored_feature_fluid_.php

        http://www.mijagourlay.com/

    \author Copyright 2009-2010 Dr. Michael Jason Gourlay; All rights reserved.
*/
#ifndef PARTICLE_OPERATION_KILL_AGE_H
#define PARTICLE_OPERATION_KILL_AGE_H

#include "particleOperation.h"

/*! \brief Particle operation to kill particles based on their age
*/
class PclOpKillAge : public IParticleOperation
{
    public:
        PclOpKillAge( void )
            : mParticles( 0 )
            , mAgeMax( 0 )
        {}

        void Operate( float timeStep , unsigned uFrame )
        {
            QUERY_PERFORMANCE_ENTER ;

            const Particle * pPcls = & (*mParticles)[ 0 ] ;
            for( size_t iPcl = 0 ;
                iPcl < mParticles->Size() ; // Note: Size cannot be cached because loop body changes size.
                /* DO NOT ++ iPcl */ )      // Note: conditional increment below.
            {   // For each particle in the given array...
                const Particle & rPcl = pPcls[ iPcl ] ;
                const int age = uFrame - rPcl.mBirthTime ;
                if( age > mAgeMax )
                {
                    Particles::Kill( * mParticles , iPcl ) ;
                }
                else
                {
                    ++ iPcl ;
                }
            }

            QUERY_PERFORMANCE_EXIT( Particles_Kill ) ;
        }

        Vector< Particle > *    mParticles              ;   ///< Dynamic array of particles to kill
        int                     mAgeMax                 ;   ///< Maximum age of particles
} ;

#endif