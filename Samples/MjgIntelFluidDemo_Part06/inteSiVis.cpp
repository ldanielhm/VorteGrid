/*! \file inteSiVis.h

    \brief Application for interactive simulation and visualization

    \see Accompanying articles for more information:
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-1/
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-2/
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-3/
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-4/
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-5/
        http://software.intel.com/en-us/articles/fluid-simulation-for-video-games-part-6/

        http://www.gamasutra.com/view/feature/4164/sponsored_feature_fluid_.php
        http://www.gamasutra.com/view/feature/4176/sponsored_feature_fluid_.php

        http://www.mijagourlay.com/

    \author Copyright 2009 Dr. Michael Jason Gourlay; All rights reserved.
*/

#include <assert.h>
#if defined( WIN32 )
    #include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glu.h>
#include "glut.h"

#if defined( _DEBUG )
    #pragma comment(lib, "glut32d.lib")
#else
    #pragma comment(lib, "glut32.lib")
#endif

#include "Core/Math/vec4.h"
#include "Core/Math/mat4.h"
#include "Core/Performance/perf.h"

#include "Sim/Vorton/vorticityDistribution.h"

#include "inteSiVis.h"

// Private variables --------------------------------------------------------------

static InteSiVis * sInstance = 0 ;
static const float  timeStep    = 1.0f / 30.0f ;

static const Vorton vortonDummy ;
static const size_t vortonStride            = sizeof( Vorton ) ;
static const size_t vortonOffsetToAngVel    = OFFSET_OF_MEMBER( vortonDummy , mVorticity ) ;
static const size_t vortonOffsetToSize      = OFFSET_OF_MEMBER( vortonDummy , mRadius ) ;

static const Particle tracerDummy ;
static const size_t tracerStride            = sizeof( Particle ) ;
static const size_t tracerOffsetToAngVel    = OFFSET_OF_MEMBER( tracerDummy , mAngularVelocity ) ;
static const size_t tracerOffsetToSize      = OFFSET_OF_MEMBER( tracerDummy , mSize ) ;

static int sMousePrevX = -999 ;
static int sMousePrevY = -999 ;

static const float gScale = 1.0f ;

bool gPrintProfileData = false ;

#if defined( PROFILE )
static int       sFrameCount    = 0     ;   // In profile builds, run for a fixed number of frames
static const int sFrameCountMax = 300   ;   // In profile builds, run for a fixed number of frames
#endif

// Functions --------------------------------------------------------------


/*! \brief Construct application for interactive simulation and visualization

    \param viscosity - fluid viscosity

    \param density - fluid density

*/
InteSiVis::InteSiVis( float viscosity , float density )
    : mFluidBodySim( viscosity , density )
#if 1
    , mCamera( 640 , 480 )  // Standard definition
#else
    , mCamera( 1280 , 720 ) // 720p high definition
#endif
    , mVortonRenderer( 0 , vortonStride , vortonOffsetToAngVel , vortonOffsetToSize )
    , mTracerRenderer( 0 , tracerStride , tracerOffsetToAngVel , tracerOffsetToSize )
    , mRenderWindow( 0 )
    , mStatusWindow( 0 )
    , mFrame( 0 )
    , mTimeNow( 0.0 )
    , mInitialized( false )
{
    sInstance = this ;
    mMouseButtons[0] = mMouseButtons[1] = mMouseButtons[2] = 0 ;

    InitialConditions( 2 ) ;

    mLights[0].mPosition = Vec3( 0.7f , -0.2f , 0.7f ) ; // actually a direction
    mLights[0].mColor    = Vec3( 1.0f , 0.5f , 0.2f ) ;
    mLights[0].mType     = QdLight::LT_DIRECTIONAL ;

    mLights[1].mPosition = Vec3( -0.7f , -0.2f , 0.7f ) ; // actually a direction
    mLights[1].mColor    = Vec3( 0.2f , 0.5f , 1.0f ) ;
    mLights[1].mType     = QdLight::LT_DIRECTIONAL ;
}




#if USE_FANCY_PARTICLES
/*! \brief Cheap lighting hack to make an internal glow effect inside smoke

    \param vGlowPos - position of point light

*/
static void SetGlow( const Vec3 vGlowPos )
{
    glEnable( GL_LIGHTING ) ;

    // Code below here belongs in "QdLight"
    // Simplify (and speed up) specular computation
    glLightModeli( GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE ) ;

    // Global ambient light
    {
        static const int iLight = GL_LIGHT0 ;
        float globalAmbientLight[] = { 1.0f , 1.0f , 1.0f , 1.0f } ;
        glLightModelfv( GL_LIGHT_MODEL_AMBIENT , globalAmbientLight ) ;
        glEnable( iLight ) ;
    }

    // Glow point light
    {
        glPushMatrix() ;
        static const int iLight = GL_LIGHT1 ;
        const Vec4 diffuse( 18.0 , 9.0 , 0.0 , 1.0 ) ;
        glLightfv( iLight , GL_DIFFUSE , (float*) & diffuse ) ;
        Vec4 pos( vGlowPos , 1.0 ) ;
        glLightfv( iLight , GL_POSITION , (float*) & pos ) ;
        glLightf( iLight , GL_CONSTANT_ATTENUATION , 0.0f ) ;
        glLightf( iLight , GL_LINEAR_ATTENUATION , 0.0f ) ;
        glLightf( iLight , GL_QUADRATIC_ATTENUATION , 10.0 ) ;
        glEnable( iLight ) ;
        glPopMatrix() ;
    }

    // Directional light from above
    {
        glPushMatrix() ;
        static const int iLight = GL_LIGHT2 ;
        const Vec4 ambient( 0.05 , 0.05 , 0.05 , 1.0 ) ;
        glLightfv( iLight , GL_AMBIENT , (float*) & ambient ) ;
        const Vec4 diffuse( 1.0 , 1.0 , 1.0 , 1.0 ) ;
        glLightfv( iLight , GL_DIFFUSE , (float*) & diffuse ) ;
        const Vec4 position( 0.0 , 0.0 , -1.0 , 0.0 ) ;
        glLightfv( iLight , GL_POSITION , (float*) & position ) ;
        glEnable( iLight ) ;
        glPopMatrix() ;
    }
}
#endif




/*! \brief Construct application for interactive simulation and visualization

    \param ic - which initial conditions to impose

    This is just a demonstration of a few initial conditions.
    There is nothing fundamental or especially important about
    the initial conditions that this routine implements.
    These are just examples.

*/
void InteSiVis::InitialConditions( unsigned ic )
{
    mScenario           = ic  ;
    sInstance->mFrame   = 0   ;
    sInstance->mTimeNow = 0.0 ;

    mFluidBodySim.Clear() ;

    static const float      fRadius         = 1.0f ;
    static const float      fThickness      = 1.0f ;
    static const float      fMagnitude      = 20.0f ;
    static const unsigned   numCellsPerDim  = 16 ;
    static const unsigned   numVortonsMax   = numCellsPerDim * numCellsPerDim * numCellsPerDim ;
    unsigned                numTracersPer   = 3 ;

    Vector< Vorton > &      vortons         = mFluidBodySim.GetVortonSim().GetVortons() ;

    srand( 1 ) ;    // Seed pseudo-random number generator to make results repeatable.

    switch( ic )
    {   // Switch on initial conditions
        case 0: // vortex ring -- vorticity in [0,1]
            AssignVorticity( vortons , 5.0f * fMagnitude , numVortonsMax , VortexRing( fRadius , fThickness , Vec3( 1.0f , 0.0f , 0.0f ) ) ) ;
            mCamera.SetTarget( Vec3( 10.f , 0.f , 0.f ) ) ;
            mCamera.SetEye( Vec3( 10.f , -10.0f , 0.0f ) ) ;
        break ;
        case 1: // "jet" vortex ring -- velocity in [0,1]
            AssignVorticity( vortons , 5.0f * fMagnitude , numVortonsMax , JetRing( fRadius , fThickness , Vec3( 1.0f , 0.0f , 0.0f ) ) ) ;
            mCamera.SetTarget( Vec3( 10.f , 0.f , 0.f ) ) ;
            mCamera.SetEye( Vec3( 10.f , -10.0f , 0.0f ) ) ;
        break ;
        case 2: // Projectile
            AssignVorticity( vortons , 0.125f * FLT_EPSILON , 2048 , VortexNoise( Vec3( 4.0f * fThickness , 0.5f * fThickness , 0.5f * fThickness ) ) ) ;
            // Add a sphere
            mFluidBodySim.GetSpheres().PushBack( RbSphere( Vec3( -2.0f , 0.0f , 0.0f ) , Vec3( 10.0f , 0.0f , 0.0f ) , 0.2f , 0.2f ) ) ;
            mFluidBodySim.GetSpheres()[0].ApplyImpulsiveTorque( Vec3( 0.0f , 0.0f , 0.0f ) ) ; // Make sphere spin
            mCamera.SetTarget( Vec3( 1.0f , 0.0f , 0.0f ) ) ;
            mCamera.SetEye( Vec3( 1.0f , -3.0f , 0.0f ) ) ;
            numTracersPer = 6 ;
        break ;
        case 3: // Projectile with spin about longitudinal axis
            AssignVorticity( vortons , 0.125f * FLT_EPSILON , 2048 , VortexNoise( Vec3( 4.0f * fThickness , 0.5f * fThickness , 0.5f * fThickness ) ) ) ;
            // Add a sphere
            mFluidBodySim.GetSpheres().PushBack( RbSphere( Vec3( -2.0f , 0.0f , 0.0f ) , Vec3( 10.0f , 0.0f , 0.0f ) , 0.2f , 0.2f ) ) ;
            mFluidBodySim.GetSpheres()[0].ApplyImpulsiveTorque( Vec3( 0.1f , 0.0f , 0.0f ) ) ; // Make sphere spin
            mCamera.SetTarget( Vec3( 1.0f , 0.0f , 0.0f ) ) ;
            mCamera.SetEye( Vec3( 1.0f , -3.0f , 0.0f ) ) ;
            numTracersPer = 6 ;
        break ;
        case 4: // Projectile with spin about transverse axis
            AssignVorticity( vortons , 0.125f * FLT_EPSILON , 2048 , VortexNoise( Vec3( 4.0f * fThickness , 0.5f * fThickness , 0.5f * fThickness ) ) ) ;
            // Add a sphere
            mFluidBodySim.GetSpheres().PushBack( RbSphere( Vec3( -2.0f , 0.0f , 0.0f ) , Vec3( 10.0f , 0.0f , 0.0f ) , 0.2f , 0.2f ) ) ;
            mFluidBodySim.GetSpheres()[0].ApplyImpulsiveTorque( Vec3( 0.0f , 0.1f , 0.0f ) ) ; // Make sphere spin
            mCamera.SetTarget( Vec3( 1.0f , 0.0f , 0.0f ) ) ;
            mCamera.SetEye( Vec3( 1.0f , -3.0f , 0.0f ) ) ;
            numTracersPer = 6 ;
        break ;
        case 5: // Spinning sphere
            AssignVorticity( vortons , 0.125f * FLT_EPSILON , 2048 , VortexNoise( Vec3( fThickness , fThickness , fThickness ) ) ) ;
            // Add a sphere
            mFluidBodySim.GetSpheres().PushBack( RbSphere( Vec3( 0.f , 0.0f , 0.0f ) , Vec3( 0.f , 0.0f , 0.0f ) , 100.0f , 0.2f ) ) ;
            mFluidBodySim.GetSpheres()[0].ApplyImpulsiveTorque( Vec3( 0.0f , 0.0f , 10.0f ) ) ; // Make sphere spin
            mCamera.SetTarget( Vec3( 0.0f , 0.0f , 0.0f ) ) ;
            mCamera.SetEye( Vec3( 0.0f , -2.0f , 0.0f ) ) ;
            numTracersPer = 6 ;
        break ;
        case 6: // Sphere in spinning fluid
            AssignVorticity( vortons , 2.0f * fMagnitude , numVortonsMax , VortexTube( fThickness , /* variation */ 0.0f , /* width */ 2.0f * fThickness , 2 , 0 ) ) ;
            // Add a sphere
            mFluidBodySim.GetSpheres().PushBack( RbSphere( Vec3( 0.f , 0.0f , 0.0f ) , Vec3( 0.f , 0.0f , 0.0f ) , 5.0f , 0.1f ) ) ;
            mCamera.SetTarget( Vec3( 0.0f , 0.0f , 0.0f ) ) ;
            mCamera.SetEye( Vec3( 0.0f , -2.0f , 0.0f ) ) ;
            numTracersPer = 6 ;
        break ;
        case 7: // Vortex sheet with spanwise variation
            AssignVorticity( vortons , fMagnitude , numVortonsMax , VortexSheet( fThickness , /* variation */ 0.2f , /* width */ 7.0f * fThickness ) ) ;
            mCamera.SetTarget( Vec3( 0.0f , 0.0f , 0.0f ) ) ;
            mCamera.SetEye( Vec3( 0.0f , -20.0f , 0.0f ) ) ;
        break ;
        case 8: // Vortex tube
            AssignVorticity( vortons , fMagnitude , numVortonsMax , VortexTube( fThickness , /* variation */ 0.0f , /* width */ 2.0f * fThickness , 2 , 0 ) ) ;
        break ;
        case 9: // 2 orthogonal vortex tubes
            AssignVorticity( vortons , fMagnitude , numVortonsMax , VortexTube( fThickness , /* variation */ 0.0f , /* width */ 4.0f * fThickness , 2 , -1 ) ) ;
            AssignVorticity( vortons , fMagnitude , numVortonsMax , VortexTube( fThickness , /* variation */ 0.0f , /* width */ 4.0f * fThickness , 2 ,  1 ) ) ;
            mCamera.SetTarget( Vec3( 0.0f , 0.0f , 0.0f ) ) ;
            mCamera.SetEye( Vec3( 0.0f , -2.0f , 0.0f ) ) ;
        break ;
        case 10: // 2D sheet
            AssignVorticity( vortons , fMagnitude , numVortonsMax , VortexSheet( fThickness , /* variation */ 0.0f , /* width */ 2.0f * fThickness ) ) ;
            mCamera.SetTarget( Vec3( 0.0f , 0.0f , 0.0f ) ) ;
            mCamera.SetEye( Vec3( 0.0f , -10.0f , 0.0f ) ) ;
        break ;
        default:
        break ;
    }

#if USE_FANCY_PARTICLES
    switch( mScenario )
    {   // For some scenarios, place a point light inside the cloud to make it seem to glow from within.
        case 0: // vortex ring -- vorticity in [0,1]
        case 1: // "jet" vortex ring -- velocity in [0,1]
            SetGlow( mFluidBodySim.GetVortonSim().GetTracerCenterOfMass() ) ;
        break ;
        default:
            QdLight::DisableLights() ;
        break ;
    }
#else   // When not using "fancy particles", disable all lighting and shading -- just use so-called "flat" shading.
    QdLight::DisableLights() ;
#endif

    mFluidBodySim.Initialize( numTracersPer ) ;
}




/*! \brief Destruct application for interactive simulation and visualization
*/
InteSiVis::~InteSiVis()
{
    sInstance = 0 ;
}




/*! \brief Function that GLUT calls when window resizes
*/
/* static */ void InteSiVis::GlutReshapeGlutCallback(int width, int height)
{
    const int   width_4 = (width / 4) * 4;
    sInstance->mCamera.SetViewport( width_4 ,height ) ;
    glViewport( 0 , 0 , sInstance->mCamera.GetWidth() , sInstance->mCamera.GetHeight() ) ;
    glutReshapeWindow( width_4 , height ) ;
}




/*! \brief Function that GLUT calls to display contents of window
*/
/* static */ void InteSiVis::GlutDisplayCallback(void)
{
    QUERY_PERFORMANCE_ENTER ;

	static GLuint ballDisplayList ;

    if( ! sInstance->mInitialized )
    {   // This is the first time this app has displayed anything.
        // Cannot generate a texture until OpenGL has been initialized.
        sInstance->mParticleMaterial.Initialize( QdMaterial::TP_NOISE_BALL , 16 , 16 , 0.5f ) ;
        sInstance->mParticleMaterial.mColor = Vec4( 1.0f , 1.0f , 1.0f , 1.0f ) ;
        sInstance->mParticleMaterial.mDepthWrite = false ;

        sInstance->mBallMaterial.Initialize( QdMaterial::TP_NOISE_OPAQUE , 32 , 32 , 0.5f ) ;
        sInstance->mBallMaterial.mColor = Vec4( 1.0f , 0.05f , 0.05f , 1.0f ) ;

        sInstance->mSkyMaterial.Initialize( QdMaterial::TP_NOISE_OPAQUE , 64 , 64 , 0.05f ) ;
        sInstance->mSkyMaterial.mColor = Vec4( 0.02f , 0.01f , 0.5f , 1.0f ) ;

	    ballDisplayList = glGenLists( 1 ) ; // Create the id for the display list
	    glNewList( ballDisplayList , GL_COMPILE ) ;
        glTexGeni( GL_S , GL_TEXTURE_GEN_MODE , GL_OBJECT_LINEAR ) ;
        glTexGeni( GL_T , GL_TEXTURE_GEN_MODE , GL_OBJECT_LINEAR ) ;
        glEnable( GL_TEXTURE_GEN_S ) ;
        glEnable( GL_TEXTURE_GEN_T ) ;
        glutSolidSphere( 1.0f , 32 , 32 ) ;
        glDisable( GL_TEXTURE_GEN_S ) ;
        glDisable( GL_TEXTURE_GEN_T ) ;
        glEndList() ;

        sInstance->mInitialized = true ;
    }
    sInstance->mCamera.SetCamera() ;

    QUERY_PERFORMANCE_ENTER ;
    // Render balls
    sInstance->mLights[0].SetLight( 0 ) ;
    sInstance->mLights[1].SetLight( 1 ) ;
    sInstance->mBallMaterial.SetMaterial() ;
    if( ! sInstance->mFluidBodySim.GetSpheres().empty() )
    {
        RbSphere & sphere = sInstance->mFluidBodySim.GetSpheres()[0] ;
        glPushMatrix() ;
    #if 1
        glTranslatef( sphere.mPosition.x , sphere.mPosition.y , sphere.mPosition.z ) ;
        if( true )
        {
            const float orientMag = sphere.mOrientation.Magnitude() ;
            if( orientMag >  TWO_PI )
            {   // orientation angle is larger than a full revolution so truncate it.
                const float orientMagNew = orientMag - TWO_PI ;
                sphere.mOrientation *= ( orientMagNew / orientMag ) ;
            }
            else if( orientMag < 0 )
            {   // orientation angle is larger than a full revolution so truncate it.
                const float orientMagNew = orientMag - TWO_PI ;
                sphere.mOrientation *= ( orientMagNew / orientMag ) ;
            }
        }
        const float angle = sphere.mOrientation.Magnitude() ;
        Vec4 rotAxis( sphere.mOrientation ) ;
        if( fabsf( angle ) > FLT_EPSILON )
        {   // angle is not near zero.  (Angle near zero would cause problems for Normalize below.)
            rotAxis.Normalizev3() ;
        }
        else
        {
            rotAxis = Vec4( 1.0f , 0.0f , 0.0f , 0.0f ) ;
        }
        glRotatef( angle * RAD2DEG , rotAxis.x , rotAxis.y , rotAxis.z ) ;
        glScalef( sphere.mRadius , sphere.mRadius , sphere.mRadius ) ;
    #else
        Mat4 xLocalToWorld( Mat4_xIdentity ) ;
        xLocalToWorld.SetTranslation( Vec4( sphere.mPosition , 1.0f ) ) ;
        glMultMatrixf( (GLfloat*) & xLocalToWorld ) ;
    #endif
		glCallList( ballDisplayList ) ;
        glPopMatrix() ;
    }

    sInstance->mSkyMaterial.SetMaterial() ;
    QdLight::DisableLights() ;
    // Render sky ball
    {
        glCullFace(GL_FRONT) ; // Render /inside/ of sphere
        glPushMatrix() ;
        glTranslatef( 0.0f , 0.0f , 0.0f ) ; // should translate to camera eye
        static const float skyRadius = 500.0f ;
        glScalef( skyRadius , skyRadius , skyRadius ) ;
		glCallList( ballDisplayList ) ;
        glPopMatrix() ;
    }
    QUERY_PERFORMANCE_EXIT( InteSiVis_Render_NonFluidObjects ) ;


    QUERY_PERFORMANCE_ENTER ;
    sInstance->mParticleMaterial.SetMaterial() ;
    QdLight::DisableLights() ;
    QUERY_PERFORMANCE_EXIT( InteSiVis_Render_SetMaterial ) ;

#if USE_FANCY_PARTICLES
    QUERY_PERFORMANCE_ENTER ;
    switch( sInstance->mScenario )
    {
        case 0: case 1:
            const Vec3      vCoM        = sInstance->mFluidBodySim.GetVortonSim().GetTracerCenterOfMass() ;
            const Vec3 &    vMin        = sInstance->mFluidBodySim.GetVortonSim().GetVelocityGrid().GetMinCorner() ;
            const Vec3 &    vExtent     = sInstance->mFluidBodySim.GetVortonSim().GetVelocityGrid().GetExtent() ;
            const Vec3      vMax        = vMin + vExtent ;
            const Vec3      vFireball   = Vec3( 0.5f * ( vCoM.x + vMax.x ) , vCoM.y , vCoM.z ) ;
            SetGlow( vFireball ) ;
        break ;
    }
    QUERY_PERFORMANCE_EXIT( InteSiVis_Render_SetGlow ) ;
#endif

    const VortonSim & rVortonSim = sInstance->mFluidBodySim.GetVortonSim() ;

    // Render vortons:
    //sInstance->mVortonRenderer.SetParticleData( (char*) & rVortonSim.GetVortons()[0] ) ;
    //sInstance->mVortonRenderer.Render( sInstance->mTimeNow , timeStep , rVortonSim.GetVortons().Size() ) ;

    // Render tracers:
    sInstance->mTracerRenderer.SetParticleData( (char*) & rVortonSim.GetTracers()[0] ) ;
    sInstance->mTracerRenderer.Render( sInstance->mTimeNow , timeStep , rVortonSim.GetTracers().Size() ) ;

    QUERY_PERFORMANCE_ENTER ;
    glutSwapBuffers() ;
    QUERY_PERFORMANCE_EXIT( InteSiVis_Render_SwapBuffers ) ;

#if PROFILE > 1
    ++ sFrameCount ;
    if( sFrameCount == sFrameCountMax )
    {   // Reached last frame, so enable profile reporting
        gPrintProfileData = true ;
    }
    else if( sFrameCount > sFrameCountMax )
    {   // Reached maximum number of frames, so exit.
        fprintf( stderr , "region: { %g , %g , %g } , { %g , %g , %g } \n"
            , sInstance->mFluidBodySim.GetVortonSim().GetMinCornerEternal().x
            , sInstance->mFluidBodySim.GetVortonSim().GetMinCornerEternal().y
            , sInstance->mFluidBodySim.GetVortonSim().GetMinCornerEternal().z
            , sInstance->mFluidBodySim.GetVortonSim().GetMaxCornerEternal().x
            , sInstance->mFluidBodySim.GetVortonSim().GetMaxCornerEternal().y
            , sInstance->mFluidBodySim.GetVortonSim().GetMaxCornerEternal().z
            ) ;
        fprintf( stderr , "Benchmark DONE: " __DATE__ " " __TIME__ "\n\n" ) ;

        exit( 0 ) ;
    }
#endif

    QUERY_PERFORMANCE_EXIT( InteSiVis_Render ) ;
}




/*! \brief Function that GLUT calls when nothing else is happening
*/
/* static */ void InteSiVis::GlutIdleCallback(void)
{
    QUERY_PERFORMANCE_ENTER ;

#if USE_TBB
    tbb::tick_count time0 = tbb::tick_count::now() ;
#endif

    QUERY_PERFORMANCE_ENTER ;
    sInstance->mFluidBodySim.Update( timeStep , sInstance->mFrame ) ;
    QUERY_PERFORMANCE_EXIT( InteSiVis_FluidBodySim_Update ) ;

    ++ sInstance->mFrame ;
    sInstance->mTimeNow += timeStep ;

#if USE_TBB
    tbb::tick_count timeFinal = tbb::tick_count::now() ;
    //fprintf( stderr , " tbb duration=%g second\n" , (timeFinal - time0).seconds() ) ;
#endif

    InteSiVis::GlutDisplayCallback() ;

    QUERY_PERFORMANCE_EXIT( InteSiVis_UPDATE_Michael_J_Gourlay_2009 ) ;
}




/*! \brief Function that GLUT calls to handle a special key
*/
/* static */ void InteSiVis::GlutSpecialKeyHandler (int key, int x, int y)
{
    QdCamera &  cam     = sInstance->mCamera ;
    Vec3        vTarget = cam.GetTarget() ;

    switch (key)
    {
        case GLUT_KEY_F1: sInstance->InitialConditions( 1 ) ; break;
        case GLUT_KEY_F2: sInstance->InitialConditions( 2 ) ; break;
        case GLUT_KEY_F3: sInstance->InitialConditions( 3 ) ; break;
        case GLUT_KEY_F4: sInstance->InitialConditions( 4 ) ; break;
        case GLUT_KEY_F5: sInstance->InitialConditions( 5 ) ; break;
        case GLUT_KEY_F6: sInstance->InitialConditions( 6 ) ; break;
        case GLUT_KEY_F7: sInstance->InitialConditions( 0 ) ; break;

        case GLUT_KEY_UP   : vTarget.z += 0.1f ; cam.SetTarget( vTarget ) ; break ;
        case GLUT_KEY_DOWN : vTarget.z -= 0.1f ; cam.SetTarget( vTarget ) ; break ;
        case GLUT_KEY_RIGHT: vTarget.x -= 1.0f ; cam.SetTarget( vTarget ) ; break ;
        case GLUT_KEY_LEFT : vTarget.x += 1.0f ; cam.SetTarget( vTarget ) ; break ;

        default:
        return;
        break;
    }
    
    glutPostRedisplay() ;
}




/*! \brief Function that GLUT calls to handle a regular key
*/
/* static */ void InteSiVis::GlutKeyboardHandler (unsigned char key, int x, int y)
{
    QdCamera & cam = sInstance->mCamera ;
    float azimuth , elevation , radius ;
    cam.GetOrbit( azimuth , elevation , radius ) ;
    switch( key )
    {
        case '?': gPrintProfileData = ! gPrintProfileData ; break ;

        case '.': radius -= 0.1f ; break ;
        case ',': radius += 0.1f ; break ;

        case 127 /* delete */ : ; break;

        case 27 /* escape */ :
            exit( 0 ) ;
        break;

        default:
            return;
            /*NOTREACHED*/
        break;
    }
    cam.SetOrbit( azimuth , elevation , radius ) ;
}




/*! \brief Function that GLUT calls to handle mouse button events
*/
/* static */ void InteSiVis::GlutMouseHandler( int button , int state , int x , int y )
{
    if( GLUT_DOWN == state )
    {   // User is clicking a mouse button
        sInstance->mMouseButtons[ button ] = 1 ;
    }
    else
    {   // User released a mouse button
        sInstance->mMouseButtons[ button ] = 0 ;
    }
    sMousePrevX = x ;
    sMousePrevY = y ;
}




/*! \brief Function that GLUT calls to handle mouse motion
*/
/* static */ void InteSiVis::GlutMouseMotionHandler( int x , int y )
{
    if( -999 == sMousePrevX )
    {
        sMousePrevX = x ;
        sMousePrevY = y ;
        return ;
    }

    static float fMouseMotionSensitivity = 0.005f ;    // Tune based on how fast you want mouse to move camera.

    const float dx = CLAMP( fMouseMotionSensitivity * float( x - sMousePrevX ) , -100.0f , 100.0f ) ;
    const float dy = CLAMP( fMouseMotionSensitivity * float( y - sMousePrevY ) , -100.0f , 100.0f ) ;

    if( sInstance->mMouseButtons[0] )
    {   // User is left-click-dragging mouse
        QdCamera & cam = sInstance->mCamera ;
        float azimuth , elevation , radius ;
        // Obtain previous camera parameters.
        cam.GetOrbit( azimuth , elevation , radius ) ;
        // Avoid gimbal lock by limiting elevation angle to avoid the poles.
        static const float sAvoidPoles = 0.001f ;
        elevation = CLAMP( elevation - dy , sAvoidPoles , PI * ( 1.0f - sAvoidPoles ) ) ;
        // Set new camera parameters based on how much mouse moved.
        cam.SetOrbit( azimuth - dx , elevation , radius ) ;
    }
    else if( sInstance->mMouseButtons[2] )
    {   // User is right-click-dragging mouse
        QdCamera & cam = sInstance->mCamera ;
        float azimuth , elevation , radius ;
        // Obtain previous camera parameters.
        cam.GetOrbit( azimuth , elevation , radius ) ;
        // Set new camera parameters based on how much mouse moved.
        cam.SetOrbit( azimuth , elevation , radius - ( dx + dy ) ) ;
    }
    else if( sInstance->mMouseButtons[1] )
    {   // User is middle-click-dragging mouse
        QdCamera & cam = sInstance->mCamera ;
        const Vec3 & rEye    = cam.GetEye() ;
        const Vec3 & rTarget = cam.GetTarget() ;
        // Extract world space direction vectors associated with view (used to compute camera-facing coordinates).
        // Note that these vectors are the unit vectors of the inverse of the view matrix.
        // They are the world-space unit vectors of the view transformation.
        Mat4 viewMatrix ;
        glGetFloatv( GL_MODELVIEW_MATRIX , (GLfloat *) & viewMatrix ) ;
        Vec3 viewRight  ( viewMatrix.m[0][0] , viewMatrix.m[1][0] , viewMatrix.m[2][0] ) ;
        Vec3 viewUp     ( viewMatrix.m[0][1] , viewMatrix.m[1][1] , viewMatrix.m[2][1] ) ;
        Vec3 viewForward( viewMatrix.m[0][2] , viewMatrix.m[1][2] , viewMatrix.m[2][2] ) ;
        Vec3 delta = 2.0f * ( - viewRight * dx + viewUp * dy ) ;
        // This ought to use the view to change position.
        cam.SetEye   ( Vec3( rEye.x    + delta.x , rEye.y    + delta.y , rEye.z    + delta.z ) ) ;
        cam.SetTarget( Vec3( rTarget.x + delta.x , rTarget.y + delta.y , rTarget.z + delta.z ) ) ;
    }

    sMousePrevX = x ;
    sMousePrevY = y ;
}




/*! \brief Function that GLUT calls when the window obtains focus
*/
/* static */ void InteSiVis::GlutEntryHandler( int state )
{
    if( GLUT_LEFT == state )
    {   // Focus left this window so act like mouse buttons got released.
        sInstance->mMouseButtons[0] =
        sInstance->mMouseButtons[1] =
        sInstance->mMouseButtons[2] = 0 ;
    }
}




typedef BOOL (APIENTRY *PFNWGLSWAPINTERVALFARPROC)( int );
PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT = 0;

void setVSync(int interval=1)
{
    const char * extensions = (char*) glGetString( GL_EXTENSIONS );

    if( strstr( extensions, "WGL_EXT_swap_control" ) == 0 )
    {   // Your computer does NOT support WGL_EXT_swap_control extension.
        return ;
    }
    else
    {
        wglSwapIntervalEXT = (PFNWGLSWAPINTERVALFARPROC)wglGetProcAddress( "wglSwapIntervalEXT" );
        if( wglSwapIntervalEXT )
        {   // Set the vsync mode
            wglSwapIntervalEXT(interval);
        }
    }
}




/*! \brief Initialize display device
*/
void InteSiVis::InitDevice( int * pArgc , char ** argv )
{
    glutInit( pArgc , argv ) ;

    unsigned int mode =     GLUT_RGBA
                        |   GLUT_DEPTH
                        |   GLUT_DOUBLE
                        //|   GLUT_ACCUM
                        //|   GLUT_STENCIL
                        //|   GLUT_MULTISAMPLE
                        ;

    // Initialize the GL utility toolkit.
    glutInitDisplayMode( mode ) ;

    // Create render window
    glutInitWindowPosition( 0 , 0 ) ;
    glutInitWindowSize( sInstance->mCamera.GetWidth() , sInstance->mCamera.GetHeight() ) ;
    mRenderWindow = glutCreateWindow( "(C) 2009 Michael J. Gourlay: InteSiVis Render Window" ) ;

    // Register GLUT callbacks for main window
    glutIdleFunc( GlutIdleCallback) ;
    glutDisplayFunc( GlutDisplayCallback) ;
    glutKeyboardFunc( GlutKeyboardHandler ) ;
    glutSpecialFunc( GlutSpecialKeyHandler ) ;
    glutReshapeFunc( GlutReshapeGlutCallback ) ;
    glutMotionFunc( GlutMouseMotionHandler ) ;
    glutMouseFunc( GlutMouseHandler ) ;
    glutPassiveMotionFunc( GlutMouseMotionHandler ) ;
    glutEntryFunc( GlutEntryHandler ) ;

    // Set the current window to the main window
    glutSetWindow( mRenderWindow ) ;

    setVSync( 0 ) ;

    glutMainLoop() ; // Relinquish control to GLUT.  This never returns.
}




int main( int argc , char ** argv )
{
    fprintf( stderr , "Benchmark: " __DATE__ " " __TIME__ "\n" ) ;

    //XMM_SetFlushToZeroDenormalAreZero() ;
    //Setx87Precision( PRECISION_SINGLE ) ;

#if PROFILE
    {   // Redirect stderr to a file to collect profile data.
        freopen( "profile.log" , "aa" , stderr ) ;
        fprintf( stderr , "Benchmark: " __DATE__ " " __TIME__ "\n" ) ;
    }
#endif

    static const float viscosity = 0.01f ;
    static const float density   = 1.0f ;
    InteSiVis inteSiVis( viscosity , density ) ;
    inteSiVis.InitDevice( & argc , argv ) ;
    return 0 ;
}
