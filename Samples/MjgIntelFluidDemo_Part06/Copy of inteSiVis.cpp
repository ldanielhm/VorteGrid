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
    ASSERT( 0 == sInstance ) ;
    sInstance = this ;
    mMouseButtons[0] = mMouseButtons[1] = mMouseButtons[2] = 0 ;

    InitialConditions( 3 ) ;

#if defined( _DEBUG )
    UniformGrid<unsigned>::UnitTest() ;
    NestedGrid<unsigned>::UnitTest() ;
    NestedGrid<Vec3>::UnitTest() ;
    extern void UnitTestPoisson1D( void ) ;
    UnitTestPoisson1D() ;
    extern void UnitTestPoisson2D( void ) ;
    UnitTestPoisson2D() ;
#endif
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
            AssignVorticity( vortons , 2.0f * fMagnitude , numVortonsMax , VortexRing( fRadius , fThickness , Vec3( 1.0f , 0.0f , 0.0f ) ) ) ;
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
            mCamera.SetTarget( Vec3( 1.0f , 0.0f , 0.0f ) ) ;
            mCamera.SetEye( Vec3( 1.0f , -3.0f , 0.0f ) ) ;
            numTracersPer = 6 ;
        break ;
        case 3: // Spinning sphere
            AssignVorticity( vortons , 0.125f * FLT_EPSILON , 2048 , VortexNoise( Vec3( fThickness , fThickness , fThickness ) ) ) ;
            // Add a sphere
            mFluidBodySim.GetSpheres().PushBack( RbSphere( Vec3( 0.f , 0.0f , 0.0f ) , Vec3( 0.f , 0.0f , 0.0f ) , 100.0f , 0.2f ) ) ;
            mFluidBodySim.GetSpheres()[0].ApplyImpulsiveTorque( Vec3( 0.0f , 0.0f , 10.0f ) ) ; // Make sphere spin
            mCamera.SetTarget( Vec3( 0.0f , 0.0f , 0.0f ) ) ;
            mCamera.SetEye( Vec3( 0.0f , -2.0f , 0.0f ) ) ;
            numTracersPer = 6 ;
        break ;
        case 4: // Vortex sheet with spanwise variation
            AssignVorticity( vortons , fMagnitude , numVortonsMax , VortexSheet( fThickness , /* variation */ 0.2f , /* width */ 7.0f * fThickness ) ) ;
            mCamera.SetTarget( Vec3( 0.0f , 0.0f , 0.0f ) ) ;
            mCamera.SetEye( Vec3( 0.0f , -20.0f , 0.0f ) ) ;
        break ;
        case 5: // Vortex tube
            AssignVorticity( vortons , fMagnitude , numVortonsMax , VortexTube( fThickness , /* variation */ 0.0f , /* width */ 2.0f * fThickness , 2 , 0 ) ) ;
        break ;
        case 6: // 2 orthogonal vortex tubes
            AssignVorticity( vortons , fMagnitude , numVortonsMax , VortexTube( fThickness , /* variation */ 0.0f , /* width */ 4.0f * fThickness , 2 , -1 ) ) ;
            AssignVorticity( vortons , fMagnitude , numVortonsMax , VortexTube( fThickness , /* variation */ 0.0f , /* width */ 4.0f * fThickness , 2 ,  1 ) ) ;
            mCamera.SetTarget( Vec3( 0.0f , 0.0f , 0.0f ) ) ;
            mCamera.SetEye( Vec3( 0.0f , -2.0f , 0.0f ) ) ;
        break ;
        case 7: // 2D sheet
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
            glDisable( GL_LIGHTING ) ;
            glDisable( GL_LIGHT0 ) ;
            glDisable( GL_LIGHT1 ) ;
            glDisable( GL_LIGHT2 ) ;
            glDisable( GL_LIGHT3 ) ;
            glDisable( GL_LIGHT4 ) ;
            glDisable( GL_LIGHT5 ) ;
            glDisable( GL_LIGHT6 ) ;
            glDisable( GL_LIGHT7 ) ;
        break ;
    }
#else   // When not using "fancy particles", disable all lighting and shading -- just use so-called "flat" shading.
    glDisable( GL_LIGHTING ) ;
    glDisable( GL_LIGHT0 ) ;
    glDisable( GL_LIGHT1 ) ;
    glDisable( GL_LIGHT2 ) ;
    glDisable( GL_LIGHT3 ) ;
    glDisable( GL_LIGHT4 ) ;
    glDisable( GL_LIGHT5 ) ;
    glDisable( GL_LIGHT6 ) ;
    glDisable( GL_LIGHT7 ) ;
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
static void GlutReshapeGlutCallback(int width, int height)
{
#if 1
	float ratio = 1.0f * width / height;
	// Reset the coordinate system before modifying
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	// Set the viewport to be the entire window
	glViewport(0, 0, width, height);
	// Set the clipping volume
	gluPerspective(45,ratio,1,1000);
#else
    const int   width_4 = (width / 4) * 4;
    sInstance->mCamera.SetViewport( width_4 ,height ) ;
    glViewport( 0 , 0 , sInstance->mCamera.GetWidth() , sInstance->mCamera.GetHeight() ) ;
    glutReshapeWindow( width_4 , height ) ;
#endif
}






#include <math.h>
#include <glut.h>

float angle=0.0,deltaAngle = 0.0,ratio;
float x=0.0f,y=1.75f,z=5.0f;
float lx=0.0f,ly=0.0f,lz=-1.0f;
GLint snowman_display_list;
int deltaMove = 0;

void changeSize(int w, int h)
{
    glDepthMask(GL_TRUE) ;  // Disable writing to the depth buffer

	// Prevent a divide by zero, when window is too short
	// (you cant make a window of zero width).
	if(h == 0)
		h = 1;
	ratio = 1.0f * w / h;
	// Reset the coordinate system before modifying
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	// Set the viewport to be the entire window
	glViewport(0, 0, w, h);
	// Set the clipping volume
	gluPerspective(45,ratio,1,1000);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(x, y, z, 
		      x + lx,y + ly,z + lz,
			  0.0f,1.0f,0.0f);
}
void drawSnowMan() {
	glColor3f(1.0f, 1.0f, 1.0f);
    // Draw Body	
	glTranslatef(0.0f ,0.75f, 0.0f);
glTexGeni(GL_S, GL_TEXTURE_GEN_MODE,GL_SPHERE_MAP);
glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
glEnable(GL_TEXTURE_GEN_S);
glEnable(GL_TEXTURE_GEN_T);
	glutSolidSphere(0.75f,20,20);
    // Draw Head
	glTranslatef(0.0f, 1.0f, 0.0f);
	glutSolidSphere(0.25f,20,20);
    // Draw Eyes
	glPushMatrix();
	glColor3f(0.0f,0.0f,0.0f);
	glTranslatef(0.05f, 0.10f, 0.18f);
	glutSolidSphere(0.05f,10,10);
	glTranslatef(-0.1f, 0.0f, 0.0f);
	glutSolidSphere(0.05f,10,10);
	glPopMatrix();
glDisable(GL_TEXTURE_GEN_S);
glDisable(GL_TEXTURE_GEN_T);

    // Draw Nose
	glColor3f(1.0f, 0.5f , 0.5f);
	glRotatef(0.0f,1.0f, 0.0f, 0.0f);
	glutSolidCone(0.08f,0.5f,10,2);

}
GLuint createDL() {
	GLuint snowManDL;
	// Create the id for the list
	snowManDL = glGenLists(1);
	// start list
	glNewList(snowManDL,GL_COMPILE);
	// call the function that contains 
	// the rendering commands
		drawSnowMan();
	// endList
	glEndList();
	return(snowManDL);
}
void initScene() {
	glEnable(GL_DEPTH_TEST);
	snowman_display_list = createDL();
}
void orientMe(float ang) {
	lx = sin(ang);
	lz = -cos(ang);
	glLoadIdentity();
	gluLookAt(x, y, z, 
		      x + lx,y + ly,z + lz,
			  0.0f,1.0f,0.0f);
}
void moveMeFlat(int i) {
	x = x + i*(lx)*0.1;
	z = z + i*(lz)*0.1;
	glLoadIdentity();
	gluLookAt(x, y, z, 
		      x + lx,y + ly,z + lz,
			  0.0f,1.0f,0.0f);
}
void drawSeveralSnowmen(void) {
	//glMatrixMode(GL_MODELVIEW);
	//glLoadIdentity();
	//gluLookAt(0.0f , 0.0f , 0.0f , 0.0f ,0.0f ,1.0f , 0.0f,1.0f,0.0f);

    // Draw ground
	glColor3f(0.9f, 0.9f, 0.9f);
	glBegin(GL_QUADS);
		glVertex3f(-1000.0f, 0.0f, -1000.0f);
		glVertex3f(-1000.0f, 0.0f,  1000.0f);
		glVertex3f( 1000.0f, 0.0f,  1000.0f);
		glVertex3f( 1000.0f, 0.0f, -1000.0f);
		//glVertex3f(-1000.0f, -1000.0f, 0.0f);
		//glVertex3f(-1000.0f,  1000.0f, 0.0f);
		//glVertex3f( 1000.0f,  1000.0f, 0.0f);
		//glVertex3f( 1000.0f, -1000.0f, 0.0f);
	glEnd();
    // Draw several SnowMen
    static const int N = 3 ;
    static const float D = 10.0f ;
	for(int i = -N; i < N; i++)
		for(int j=-N; j < N; j++)
		    for(int k=-N; k < N; k++) {
			    glPushMatrix();
			    glTranslatef(i*D,j * D,k * D);
                drawSnowMan() ;
    //			glCallList(snowman_display_list);;
			    glPopMatrix();
		    }
}
static void GlutDisplayCallback2(void);

void renderScene(void) {
	if (deltaMove)
		moveMeFlat(deltaMove);
	if (deltaAngle) {
		angle += deltaAngle;
		orientMe(angle);
	}
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_TRUE) ;  // Disable writing to the depth buffer

    drawSeveralSnowmen() ;
GlutDisplayCallback2();
	glutSwapBuffers();
    glFlush();
}
void pressKey(int key, int x, int y) {
	switch (key) {
		case GLUT_KEY_LEFT : 
			deltaAngle = -0.01f;break;
		case GLUT_KEY_RIGHT : 
			deltaAngle = 0.01f;break;
		case GLUT_KEY_UP : 
			deltaMove = 1;break;
		case GLUT_KEY_DOWN : 
			deltaMove = -1;break;
	}
}
void releaseKey(int key, int x, int y) {
	switch (key) {
		case GLUT_KEY_LEFT : 
		case GLUT_KEY_RIGHT : 
			deltaAngle = 0.0f;break;
		case GLUT_KEY_UP : 
		case GLUT_KEY_DOWN : 
			deltaMove = 0;break;
	}
}












/*! \brief Function that GLUT calls to display contents of window
*/
static void GlutDisplayCallback2(void)
{
    QUERY_PERFORMANCE_ENTER ;

    if( ! sInstance->mInitialized )
    {   // This is the first time this app has displayed anything
        sInstance->mParticleMaterial.Initialize() ;
        sInstance->mInitialized = true ;
    }
    //sInstance->mCamera.SetCamera() ;

    QUERY_PERFORMANCE_ENTER ;
    sInstance->mParticleMaterial.SetMaterial() ;
    QUERY_PERFORMANCE_EXIT( InteSiVis_Render_SetMaterial ) ;

    const VortonSim & rVortonSim = sInstance->mFluidBodySim.GetVortonSim() ;

    // Render vortons:
    //sInstance->mVortonRenderer.SetParticleData( (char*) & rVortonSim.GetVortons()[0] ) ;
    //sInstance->mVortonRenderer.Render( sInstance->mTimeNow , timeStep , rVortonSim.GetVortons().Size() ) ;

    // Render tracers:
    sInstance->mTracerRenderer.SetParticleData( (char*) & rVortonSim.GetTracers()[0] ) ;
    sInstance->mTracerRenderer.Render( sInstance->mTimeNow , timeStep , rVortonSim.GetTracers().Size() ) ;

    QUERY_PERFORMANCE_ENTER ;
    glutSwapBuffers() ;
    glFlush();
    QUERY_PERFORMANCE_EXIT( InteSiVis_Render_SwapBuffers ) ;

    QUERY_PERFORMANCE_EXIT( InteSiVis_Render ) ;
}

static void GlutDisplayCallback(void)
{
#if 0
    renderScene();
    glutSwapBuffers() ;

#else
    QUERY_PERFORMANCE_ENTER ;

    if( ! sInstance->mInitialized )
    {   // This is the first time this app has displayed anything
        sInstance->mParticleMaterial.Initialize() ;
        sInstance->mInitialized = true ;
    }
    sInstance->mCamera.SetCamera() ;

glEnable( GL_TEXTURE_2D ) ; // When rendering points, disable textures
glBindTexture( GL_TEXTURE_2D , 0 ) ;
drawSeveralSnowmen() ;


    QUERY_PERFORMANCE_ENTER ;
    sInstance->mParticleMaterial.SetMaterial() ;
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
    //glutSwapBuffers() ;
    glFlush();
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
#endif
}




/*! \brief Function that GLUT calls when nothing else is happening
*/
void GlutIdleCallback(void)
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

    GlutDisplayCallback() ;

    QUERY_PERFORMANCE_EXIT( InteSiVis_UPDATE_Michael_J_Gourlay_2009 ) ;
}




/*! \brief Function that GLUT calls to handle a special key
*/
static void GlutSpecialKeyHandler (int key, int x, int y)
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
void GlutKeyboardHandler (unsigned char key, int x, int y)
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
void GlutMouseHandler( int button , int state , int x , int y )
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
void GlutMouseMotionHandler( int x , int y )
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
void GlutEntryHandler( int state )
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
#if 1
	glutInit(pArgc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(100,100);
	glutInitWindowSize(640,360);
	glutCreateWindow("SnowMen from 3D-Tech");

	initScene();

	glutIgnoreKeyRepeat(1);
	glutSpecialFunc(pressKey);
	glutSpecialUpFunc(releaseKey);

	glutDisplayFunc(renderScene);
	glutIdleFunc(renderScene);

	glutReshapeFunc(changeSize);

	glutMainLoop();
#else
    glutInit( pArgc , argv ) ;

    unsigned int mode =     GLUT_RGBA
                        |   GLUT_DEPTH
                        //|   GLUT_DOUBLE
                        //|   GLUT_ACCUM
                        //|   GLUT_STENCIL
                        //|   GLUT_MULTISAMPLE
                        ;

    // Initialize the GL utility toolkit.
    glutInitDisplayMode( mode ) ;

    // Create render window
    glutInitWindowPosition( 10 , 10 ) ;
    glutInitWindowSize( sInstance->mCamera.GetWidth() , sInstance->mCamera.GetHeight() ) ;
    mRenderWindow = glutCreateWindow( "(C) 2009 Michael J. Gourlay: InteSiVis Render Window" ) ;

    // Register GLUT callbacks for main window
    glutIdleFunc( GlutIdleCallback) ;
    glutDisplayFunc( GlutDisplayCallback) ;
//glutDisplayFunc(renderScene);
//glutIdleFunc(renderScene);

    //glutKeyboardFunc( GlutKeyboardHandler ) ;
    //glutSpecialFunc( GlutSpecialKeyHandler ) ;
    glutReshapeFunc( GlutReshapeGlutCallback ) ;
//glutReshapeFunc(changeSize);

    //glutMotionFunc( GlutMouseMotionHandler ) ;
    //glutMouseFunc( GlutMouseHandler ) ;
    //glutPassiveMotionFunc( GlutMouseMotionHandler ) ;
    //glutEntryFunc( GlutEntryHandler ) ;

    // Set the current window to the main window
    //glutSetWindow( mRenderWindow ) ;

    //setVSync( 0 ) ;

    glutMainLoop() ; // Relinquish control to GLUT.  This never returns.
#endif
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
