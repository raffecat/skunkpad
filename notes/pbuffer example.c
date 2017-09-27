
NB. posted in a forum, not working code.

#define MAX_ATTRIBS 8
#define MAX_PFORMATS 8
static int PB_WIDTH  = 1536;
static int PB_HEIGHT = 1536;

int SaveOffscreen(GLvoid) {
	/* Get Address of wglARB-Function*/
	PFNWGLGETEXTENSIONSSTRINGARBPROC   wglGetExtensionsStringARB;
	wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress( "wglGetExtensionsStringARB" );
	if(wglGetExtensionsStringARB == NULL)
		return FALSE;

	/* Check if PBuffer-Extension is supported*/
	HDC cdc = wglGetCurrentDC();
	const char *extensions = wglGetExtensionsStringARB( cdc );
	if(!(strstr(extensions,"WGL_ARB_pixel_format") &&
		  strstr(extensions,"WGL_ARB_pbuffer")) )
		return FALSE;
	/* Initialize WGL_ARB_pbuffer entry points. */
	INIT_ENTRY_POINT(	wglCreatePbufferARB		, PFNWGLCREATEPBUFFERARBPROC );
	INIT_ENTRY_POINT(	wglGetPbufferDCARB		, PFNWGLGETPBUFFERDCARBPROC );
	INIT_ENTRY_POINT(	wglReleasePbufferDCARB	, PFNWGLRELEASEPBUFFERDCARBPROC );
	INIT_ENTRY_POINT(	wglDestroyPbufferARB		, PFNWGLDESTROYPBUFFERARBPROC );
	INIT_ENTRY_POINT(	wglQueryPbufferARB		, PFNWGLQUERYPBUFFERARBPROC );
	/* Initialize WGL_ARB_pixel_format entry points. */
	INIT_ENTRY_POINT(	wglGetPixelFormatAttribivARB	,	PFNWGLGETPIXELFORMATATTRIBIVARBPROC );
	INIT_ENTRY_POINT(	wglGetPixelFormatAttribfvARB	,	PFNWGLGETPIXELFORMATATTRIBFVARBPROC );
	INIT_ENTRY_POINT(	wglChoosePixelFormatARB			,	PFNWGLCHOOSEPIXELFORMATARBPROC );

	// Get ready to query for a suitable pixel format that meets our

	// minimum requirements.

	int iattributes[2*MAX_ATTRIBS];
	float fattributes[2*MAX_ATTRIBS];
	int nfattribs = 0;
	int niattribs = 0;
	// Attribute arrays must be '0' terminated - for simplicity, first

	// just zero-out the array then fill from left to right.

	for ( int a = 0; a < 2*MAX_ATTRIBS; a++ )
	{
	iattributes[a] = 0;
	fattributes[a] = 0;
	}
	// Since we are trying to create a pbuffer, the pixel format we

	// request (and subsequently use) must be "p-buffer capable".

	iattributes[2*niattribs ] = WGL_DRAW_TO_PBUFFER_ARB;
	iattributes[2*niattribs+1] = true;
	niattribs++;
	// We require a minimum of 24-bit depth.

	iattributes[2*niattribs ] = WGL_DEPTH_BITS_ARB;
	iattributes[2*niattribs+1] = 0;
	niattribs++;
	// We require a minimum of 8-bits for each R, G, B, and A.

	iattributes[2*niattribs ] = WGL_RED_BITS_ARB;
	iattributes[2*niattribs+1] = 8;
	niattribs++;
	iattributes[2*niattribs ] = WGL_GREEN_BITS_ARB;
	iattributes[2*niattribs+1] = 8;
	niattribs++;
	iattributes[2*niattribs ] = WGL_BLUE_BITS_ARB;
	iattributes[2*niattribs+1] = 8;
	niattribs++;
	iattributes[2*niattribs ] = WGL_ALPHA_BITS_ARB;
	iattributes[2*niattribs+1] = 0;
	niattribs++;
	// Now obtain a list of pixel formats that meet these minimum

	// requirements.

	int pformat[MAX_PFORMATS];
	unsigned int nformats;
	if ( !wglChoosePixelFormatARB( cdc, iattributes, fattributes, MAX_PFORMATS, pformat, &nformats ) ) {
		fprintf( stderr, "pbuffer creation error: Couldn't find a suitable pixel format.\n" );
		return FALSE;
	}
	int pAttrib[] =
	{
		WGL_PBUFFER_LARGEST_ARB, true,
		0 // zero terminates the list

	};
	// Choose desired Pixelformat

	HPBUFFERARB hbuffer = wglCreatePbufferARB( cdc, 1, PB_WIDTH, PB_HEIGHT, 0 );
	if(!hbuffer) {
		fprintf( stderr, "pbuffer creation error: Couldn't create PixelBuffer.\n" );
		return FALSE;
	}
	wglQueryPbufferARB( hbuffer, WGL_PBUFFER_WIDTH_ARB, &PB_WIDTH );
	wglQueryPbufferARB( hbuffer, WGL_PBUFFER_HEIGHT_ARB, &PB_HEIGHT );

	// Create a DeviceContext for the new acquired pBuffer

	HDC hpbufdc = wglGetPbufferDCARB( hbuffer );
	if(!hpbufdc) {
		fprintf( stderr, "pbuffer creation error: Couldn't acquire a Device Context for PixelBuffer.\n" );
		return FALSE;
	}

	HGLRC pbufglctx = wglCreateContext( hpbufdc );
	if(!pbufglctx) {
		fprintf( stderr, "pbuffer creation error: Couldn't acquire a Render Context for PixelBuffer.\n" );
		return FALSE;
	}

	// Switch focus to PBuffer

	wglMakeCurrent( hpbufdc, pbufglctx );

	// Render once to PBuffer and save to disk

	//glDrawBuffer(GL_FRONT);

	ReSizeGLScene(PB_WIDTH, PB_HEIGHT);
	DrawGLScene();
	//DrawGLScene();


	unsigned char *screen = (unsigned char *)malloc( PB_WIDTH * PB_HEIGHT );
	//glReadBuffer(GL_FRONT);

	glReadPixels(0, 0, PB_WIDTH, PB_HEIGHT, GL_LUMINANCE, GL_UNSIGNED_BYTE, screen);
	FILE *outfile = fopen("screenshot.dat","wb");
	fwrite(screen, PB_WIDTH, PB_HEIGHT, outfile);
	fflush(outfile);
	fclose(outfile);
	free(screen);

	// Change back to our main Window

	wglMakeCurrent( hDC, hRC );
	// Destroy PBuffer

	wglDeleteContext( pbufglctx );
	wglReleasePbufferDCARB( hbuffer, hpbufdc );
	wglDestroyPbufferARB( hbuffer );

	return TRUE;
}
