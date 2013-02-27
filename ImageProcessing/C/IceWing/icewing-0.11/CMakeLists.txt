cmake_minimum_required(VERSION 2.4.4)

IF(COMMAND cmake_policy)
    CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND cmake_policy)

INCLUDE(CheckIncludeFile)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckSymbolExists)
INCLUDE(CheckTypeSize)
MARK_AS_ADVANCED(CMAKE_BACKWARDS_COMPATIBILITY)
MARK_AS_ADVANCED(EXECUTABLE_OUTPUT_PATH)
MARK_AS_ADVANCED(LIBRARY_OUTPUT_PATH)

###
### Project name, features, and destination
###

PROJECT(iceWing)
SET(PROGNAME icewing)
SET(PACKET iceWing)
SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(BINDIR "${PREFIX}/bin")
SET(INCLUDEDIR_B "${PREFIX}/include")
SET(INCLUDEDIR "${INCLUDEDIR_B}/${PACKET}")
SET(MANDIR "${PREFIX}/man")
SET(LIBDIR_B "${PREFIX}/lib")
SET(LIBDIR "${LIBDIR_B}/${PACKET}")
SET(DATADIR "${PREFIX}/share/${PACKET}")

SET(SOURCE_DIR ${${PACKET}_SOURCE_DIR})
SET(BINARY_DIR ${${PACKET}_BINARY_DIR})

IF(SOURCE_DIR STREQUAL BINARY_DIR)
    MESSAGE(FATAL_ERROR
	"Using cmake from this in-tree directory is not supported.\n"
	"Please change to e.g. the cmake/ directory and and rerun with 'cmake ..'.\n")
ENDIF(SOURCE_DIR STREQUAL BINARY_DIR)

OPTION(WITH_GRABBER "Support grabbing hardware" 1)
OPTION(WITH_GTK1 "Force using GTK+ V1.2 for the GUI" 0)
OPTION(IW_DEBUG "Enable debugging code in ${PACKET}" 1)
OPTION(WITH_DACS "Build with DACS support if DACS is installed" 1)
OPTION(WITH_DOC "Build the iceWing PDF manual" 1)
OPTION(WITH_XCF "Build XCF communication support" 1)
OPTION(WITH_PNG "Build PNG image saving support" 1)
OPTION(WITH_JPEG "Build JPEG image saving support" 1)
OPTION(WITH_ARAVIS "Build aravis grabbing support" 1)
OPTION(WITH_FIRE2 "Build Firewire grabbing support" 1)
OPTION(WITH_ICUBE "Build iCube grabbing support" 1)
OPTION(WITH_UNICAP "Build unicap grabbing support" 1)
OPTION(WITH_MVIMPACT "Build mvDeviceManager grabbing support" 1)
OPTION(WITH_UEYE "Build IDS uEye grabbing support" 1)
OPTION(WITH_LIBV4L "Use libv4lconvert in the v4l2 grabbing driver" 1)
OPTION(WITH_FFMPEG "Build extended movie loading/saving support" 1)
OPTION(WITH_READLINE "Build icewing-control command line tool" 1)

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
	"Type of build [None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel]."
	FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

###
### Sources
###

SET(SOURCES
    main/grab.c
    main/grab_avgui.c
    main/icewing.c
    main/image.c
    main/mainloop.c
    main/output.c
    main/output_sfb.c
    main/plugin.c
    main/plugin_comm.c
    main/plugin_gui.c
    main/plugin_gui_plug.c
    main/regcalc.c
    main/region.c
    main/plugin_cxx.cpp
    gui/Gcolor.c
    gui/Gdata.c
    gui/Gdialog.c
    gui/Ggui.c
    gui/Gimage.c
    gui/Ginfo.c
    gui/Glist.c
    gui/Gmovie.c
    gui/Goptions.c
    gui/Gpolygon.c
    gui/Gpreferences.c
    gui/Gpreview.c
    gui/Grender.c
    gui/GrenderImg.c
    gui/GrenderText.c
    gui/Gsession.c
    gui/Gtools.c
    gui/text.c
    gui/avi/avilib.c
    avvideo/AVOptions.c
    tools/fileset.c
    tools/img_video.c
    tools/tools.c
    tools/opencv.cpp
    plugins/gsimple/gsimple_image.c
    plugins/gsimple/skin.c
    plugins/gsimple/gauss_simple.c
    plugins/backpro/backpro.c
    plugins/imgclass/imgclass.c
    plugins/tracking/handtrack.c
    plugins/tracking/kalman.c
    plugins/tracking/matrix.c
    plugins/tracking/track_tools.c
    plugins/tracking/trajectory.c
    plugins/tracking/track_kalman.c
    plugins/tracking/track_simple.c
    plugins/skinclass/sclas_image.c
    plugins/skinclass/skinclass.c
    plugins/polynom/polynom.c
    plugins/record/record.c
    )

###
### Support macros
###

MACRO(ERROR msg)
    MESSAGE(FATAL_ERROR ${ARGV}
	"\n"
	"If single libraries could not be found (see lines starting with '**' for any\n"
	"problems), you can specify with the environment variables 'CMAKE_INCLUDE_PATH'\n"
	"and 'CMAKE_LIBRARY_PATH' where cmake should search for them.\n")
ENDMACRO(ERROR msg)

FIND_PROGRAM(PKG_CONFIG NAMES "pkg-config" PATHS /usr/local/bin)
MARK_AS_ADVANCED(PKG_CONFIG)

# Run pkg-config to search for ${pkgargs}
MACRO(PKG_FIND pkgargs pkgwith pkgcflags pkglibs)
    SET(${pkgwith} 0)
    IF(PKG_CONFIG)
	SET(pkgold_path 0)
	EXEC_PROGRAM(${PKG_CONFIG} ARGS --cflags ${pkgargs}
	    OUTPUT_VARIABLE ${pkgcflags} RETURN_VALUE RET1)
	EXEC_PROGRAM(${PKG_CONFIG} ARGS --libs ${pkgargs}
	    OUTPUT_VARIABLE ${pkglibs} RETURN_VALUE RET2)
	IF(RET1 OR RET2)
	    # Not found ->
	    # Additionally check the directories from CMAKE_LIBRARY_PATH
	    STRING(REGEX REPLACE "/lib(32|64|)" "/lib\\1/pkgconfig" pkgconfig "$ENV{CMAKE_LIBRARY_PATH}")
	    IF(pkgconfig)
		SET(pkgold_path "$ENV{PKG_CONFIG_PATH}")
		SET(ENV{PKG_CONFIG_PATH} "${pkgconfig}:$ENV{PKG_CONFIG_PATH}")
		EXEC_PROGRAM(${PKG_CONFIG} ARGS --cflags ${pkgargs}
		    OUTPUT_VARIABLE ${pkgcflags} RETURN_VALUE RET1)
		EXEC_PROGRAM(${PKG_CONFIG} ARGS --libs ${pkgargs}
		    OUTPUT_VARIABLE ${pkglibs} RETURN_VALUE RET2)
	    ENDIF(pkgconfig)
	ENDIF(RET1 OR RET2)
	IF(NOT RET1 AND NOT RET2)
	    SET(${pkgwith} 1)
	    STRING(REGEX REPLACE "^ *\n" "" ${pkgcflags} ${${pkgcflags}})
	    STRING(REGEX REPLACE "^[ ']*([^ ']+).*" "\\1" pkgmodule ${pkgargs})
	    EXEC_PROGRAM(${PKG_CONFIG}
		ARGS --variable=prefix ${pkgmodule}
		OUTPUT_VARIABLE pkgroot)
	    MESSAGE(STATUS "Found ${pkgmodule}: ${pkgroot}")
	ELSE(NOT RET1 AND NOT RET2)
	    SET(${pkgcflags} "")
	    SET(${pkglibs} "")
	ENDIF(NOT RET1 AND NOT RET2)
	IF(pkgold_path)
	    SET(ENV{PKG_CONFIG_PATH} "${pkgold_path}")
	ENDIF(pkgold_path)
    ELSE(PKG_CONFIG)
	MESSAGE("** pkg-config not found.")
    ENDIF(PKG_CONFIG)
ENDMACRO(PKG_FIND)

# Run the config programm ${prg} to search for CFLAGS and LIBS
MACRO(CONFIG_FIND prg pargs pwith pcflags plibs)
    SET(PRG NOTFOUND)
    FIND_PROGRAM(PRG NAMES ${prg} PATHS /usr/local/bin)
    IF(NOT PRG)
	# Not found ->
	# Additionally check the directories from CMAKE_LIBRARY_PATH
	STRING(REGEX REPLACE "/lib(32|64|)" "/bin" ppath "$ENV{CMAKE_LIBRARY_PATH}")
	IF(ppath)
	    FIND_PROGRAM(PRG NAMES ${prg} PATHS "${ppath}")
	ENDIF(ppath)
    ENDIF(NOT PRG)
    IF(PRG)
	SET(${pwith} 1)
	EXEC_PROGRAM(${PRG} ARGS "--cflags ${pargs}"
	    OUTPUT_VARIABLE ${pcflags})
	EXEC_PROGRAM(${PRG} ARGS "--libs ${pargs}"
	    OUTPUT_VARIABLE ${plibs})
	EXEC_PROGRAM(${PRG} ARGS "--prefix"
	    OUTPUT_VARIABLE proot)
	MESSAGE(STATUS "Found ${prg}: ${proot}")
    ELSE(PRG)
	SET(${pwith} 0)
	SET(${pcflags} "")
	SET(${plibs} "")
    ENDIF(PRG)
ENDMACRO(CONFIG_FIND)

###
### Features
###

CHECK_INCLUDE_FILE("wordexp.h" HAVE_WORDEXP)
CHECK_FUNCTION_EXISTS(vsscanf HAVE_VSSCANF)
CHECK_SYMBOL_EXISTS(memalign "malloc.h" HAVE_MEMALIGN)
CHECK_SYMBOL_EXISTS(_mm_malloc "mm_malloc.h" HAVE_MMMALLOC)
CHECK_TYPE_SIZE(uchar HAVE_UCHAR)

###
### Platformspecific settings
###

# Alpha
IF(CMAKE_SYSTEM MATCHES "OSF1-V.*")
    IF(NOT CMAKE_COMPILER_IS_GNUCC AND NOT CMAKE_COMPILER_IS_GNUCXX)
	SET(C_FLAGS "${C_FLAGS} -std1 -w0")
	# Set path to the cxx_repository (since cmake changes dir for linking)
	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ptr ${SOURCE_DIR}/cxx_repository")
    ENDIF(NOT CMAKE_COMPILER_IS_GNUCC AND NOT CMAKE_COMPILER_IS_GNUCXX)
ENDIF(CMAKE_SYSTEM MATCHES "OSF1-V.*")

IF(CMAKE_COMPILER_IS_GNUCC)
    ADD_DEFINITIONS(-Wall)
ENDIF(CMAKE_COMPILER_IS_GNUCC)

###
### Compiler and Linker flags and Sources depending on settings
###

FIND_LIBRARY(X11_LIBRARIES NAMES X11 PATHS /usr/X11R6/lib /usr/local/lib /usr/openwin/lib /usr/lib)
IF(X11_LIBRARIES)
    MESSAGE(STATUS "Found X11: ${X11_LIBRARIES}")
ELSE(X11_LIBRARIES)
    ERROR("Could not find X11 library.\n"
	"        X11 is required to use ${PACKET}.\n")
ENDIF(X11_LIBRARIES)
MARK_AS_ADVANCED(X11_LIBRARIES)

# WITH_GTK
IF(NOT WITH_GTK1)
    PKG_FIND("gtk+-2.0 gthread-2.0" WITH_GTK2 GTK_CFLAGS GTK_LDLIBS)
    IF(WITH_GTK2)
	SET(GTK_CFLAGS "${GTK_CFLAGS} -DGTK_ENABLE_BROKEN")
    ELSE(WITH_GTK2)
	SET(WITH_GTK1 1)
	MESSAGE("** GTK+-2 library not found, trying GTK+ V1.2...")
    ENDIF(WITH_GTK2)
ENDIF(NOT WITH_GTK1)
IF(WITH_GTK1)
    CONFIG_FIND("gtk-config" "gthread" WITH_GTK1 GTK_CFLAGS GTK_LDLIBS)
    IF(NOT WITH_GTK1)
	ERROR("Could not find GTK+-1.2 library.\n"
	      "        GTK+ V1.2 or GTK+ V2.x is required to use ${PACKET}.\n")
    ENDIF(NOT WITH_GTK1)
ENDIF(WITH_GTK1)

# WITH_FFMPEG
IF(WITH_FFMPEG)
    PKG_FIND("libavformat libavcodec libavutil libswscale" WITH_FFMPEG FFMPEG_CFLAGS FFMPEG_LDLIBS)
    IF(WITH_FFMPEG)
	CHECK_INCLUDE_FILE("libavformat/avformat.h" FFMPEG_LIBINC ${FFMPEG_CFLAGS})
    ELSE(WITH_FFMPEG)
	MESSAGE("** FFmpeg libraries not found.\n"
		"   Extended movie loading/saving support will not be built.")
    ENDIF(WITH_FFMPEG)
ELSE(WITH_FFMPEG)
    MESSAGE("** FFmpeg support disabled.\n"
	    "   Extended movie loading/saving support will not be built.")
ENDIF(WITH_FFMPEG)

# WITH_JPEG
IF(WITH_JPEG)
    FIND_PACKAGE(JPEG)
    IF(JPEG_FOUND)
	SET(WITH_JPEG 1)
	SET(SOURCES ${SOURCES} gui/avi/jpegutils.c)
    ELSE(JPEG_FOUND)
	SET(WITH_JPEG 0)
	MESSAGE("** JPEG library not found.\n"
		"   JPEG saving support will not be built.")
    ENDIF(JPEG_FOUND)
ELSE(WITH_JPEG)
    MESSAGE("** JPEG support disabled.\n"
	    "   JPEG saving support will not be built.")
ENDIF(WITH_JPEG)

# WITH_PNG
IF(WITH_PNG)
    FIND_PACKAGE(PNG)
    IF(NOT PNG_FOUND)
	SET(WITH_PNG 0)
	MESSAGE("** PNG library not found.\n"
		"   PNG saving/extended loading support will not be built.")
    ENDIF(NOT PNG_FOUND)
ELSE(WITH_PNG)
    MESSAGE("** PNG support disabled.\n"
	    "   PNG saving/extended loading support will not be built.")
ENDIF(WITH_PNG)

# WITH_GPB
SET(WITH_GPB 1)
IF(WITH_GTK1)
    CONFIG_FIND("gdk-pixbuf-config" "" WITH_GPB GDK_PIXBUF_CFLAGS GDK_PIXBUF_LDLIBS)
    IF(NOT WITH_GPB)
	MESSAGE("** GDK-Pixbuf library not found.\n"
	        "   Extended image loading support will not be built.")
    ENDIF(NOT WITH_GPB)
ENDIF(WITH_GTK1)

# WITH_GRABBER
IF(WITH_GRABBER)
    IF(NOT CMAKE_SYSTEM MATCHES "Linux*")
	FIND_PATH(AV_INCLUDE_DIR AVImg.h /usr/local/include)
	FIND_LIBRARY(AV_LIBRARIES NAMES AVVideo PATHS /usr/local/lib)
	IF(AV_INCLUDE_DIR AND AV_LIBRARIES)
	    SET(AV_LIBRARIES mme ${AV_LIBRARIES})
	    MESSAGE(STATUS "Found AV: ${AV_LIBRARIES}")
	ELSE(AV_INCLUDE_DIR AND AV_LIBRARIES)
	    SET(WITH_GRABBER 0)
	    SET(AV_INCLUDE_DIR "")
	    SET(AV_LIBRARIES "")
	    MESSAGE("** AV library not found.\n"
		    "   Grabbing support from a camera will not be built.")
	ENDIF(AV_INCLUDE_DIR AND AV_LIBRARIES)
    ELSE(NOT CMAKE_SYSTEM MATCHES "Linux*")
	SET(AV_INCLUDE_DIR ${SOURCE_DIR}/avvideo)
	SET(SOURCES ${SOURCES}
	    avvideo/AVCapture.c
	    avvideo/AVColor.c
	    avvideo/AVImg.c
	    avvideo/drv_aravis.c
	    avvideo/drv_firewire.c
	    avvideo/drv_icube.c
	    avvideo/drv_unicap.c
	    avvideo/drv_mvimpact.c
	    avvideo/drv_ueye.c
	    avvideo/drv_v4l2.c
	    avvideo/drv_v4l2_old.c)

	# WITH_ARAVIS
	IF(WITH_ARAVIS)
	    PKG_FIND("'aravis-0.2 >= 0.1.14'" WITH_ARAVIS ARAVIS_CFLAGS ARAVIS_LDLIBS)
	    IF(WITH_ARAVIS)
		SET(AV_CFLAGS ${AV_CFLAGS} ${ARAVIS_CFLAGS})
		SET(AV_LDLIBS ${AV_LDLIBS} ${ARAVIS_LDLIBS})
	    ELSE(WITH_ARAVIS)
		MESSAGE("** Aravis library >=0.1.14 not found.\n"
			"   Grabbing support via aravis will not be built.")
	    ENDIF(WITH_ARAVIS)
	ELSE(WITH_ARAVIS)
	    MESSAGE("** ARAVIS support disabled.\n"
		    "   Grabbing support via aravis will not be built.")
	ENDIF(WITH_ARAVIS)

	IF(WITH_FIRE2)
	    # WITH_FIRE
	    PKG_FIND("libdc1394-2 libraw1394" WITH_FIRE2 DC_CFLAGS DC_LDLIBS)
	    IF(WITH_FIRE2)
		SET(AV_CFLAGS ${AV_CFLAGS} ${DC_CFLAGS})
		SET(AV_LDLIBS ${AV_LDLIBS} ${DC_LDLIBS})
	    ELSE(WITH_FIRE2)
		FIND_PATH(DC_INCLUDE_DIR libdc1394/dc1394_control.h /usr/local/include)
		FIND_LIBRARY(DC_LIBRARY NAMES dc1394_control PATHS /usr/local/lib)
		FIND_PATH(RAW_INCLUDE_DIR libraw1394/raw1394.h /usr/local/include)
		FIND_LIBRARY(RAW_LIBRARY NAMES raw1394 PATHS /usr/local/lib)
		IF(DC_INCLUDE_DIR AND DC_LIBRARY AND RAW_INCLUDE_DIR AND RAW_LIBRARY)
		    SET(AV_INCLUDE_DIR ${AV_INCLUDE_DIR} ${DC_INCLUDE_DIR} ${RAW_INCLUDE_DIR})
		    SET(AV_LIBRARIES ${AV_LIBRARIES} ${DC_LIBRARY} ${RAW_LIBRARY})
		    MESSAGE(STATUS "Found raw1394: ${RAW_LIBRARY}")
		    MESSAGE(STATUS "Found dc1394_control: ${DC_LIBRARY}")
		    SET(WITH_FIRE 1)
		ELSE(DC_INCLUDE_DIR AND DC_LIBRARY AND RAW_INCLUDE_DIR AND RAW_LIBRARY)
		    MESSAGE("** dc1394 or dc1394_control and raw1394 libraries not found.\n"
			    "   Grabbing support via firewire will not be built.")
		ENDIF(DC_INCLUDE_DIR AND DC_LIBRARY AND RAW_INCLUDE_DIR AND RAW_LIBRARY)
		MARK_AS_ADVANCED(DC_INCLUDE_DIR DC_LIBRARY RAW_INCLUDE_DIR RAW_LIBRARY)
	    ENDIF(WITH_FIRE2)
	ELSE(WITH_FIRE2)
	    MESSAGE("** Firewire support disabled.\n"
		    "   Grabbing support via firewire will not be built.")
	ENDIF(WITH_FIRE2)

	# WITH_ICUBE
	IF(WITH_ICUBE)
	    FIND_PATH(ICUBE_INCLUDE_DIR NETUSBCAM_API.h /usr/local/include)
	    FIND_LIBRARY(ICUBE_LIBRARY NAMES NETUSBCAM PATHS /usr/local/lib)
	    IF(ICUBE_INCLUDE_DIR AND ICUBE_LIBRARY)
		SET(AV_INCLUDE_DIR ${AV_INCLUDE_DIR} ${ICUBE_INCLUDE_DIR})
		SET(AV_LIBRARIES ${AV_LIBRARIES} ${ICUBE_LIBRARY})
		MESSAGE(STATUS "Found iCube: ${ICUBE_LIBRARY}")
	    ELSE(ICUBE_INCLUDE_DIR AND ICUBE_LIBRARY)
		SET(WITH_ICUBE 0)
		MESSAGE("** iCube library not found.\n"
			"   Grabbing support via NET iCube will not be built.")
	    ENDIF(ICUBE_INCLUDE_DIR AND ICUBE_LIBRARY)
	    MARK_AS_ADVANCED(ICUBE_INCLUDE_DIR ICUBE_LIBRARY)
	ELSE(WITH_ICUBE)
	    MESSAGE("** iCube support disabled.\n"
		    "   Grabbing support via NET iCube will not be built.")
	ENDIF(WITH_ICUBE)

	# WITH_UNICAP
	IF(WITH_UNICAP)
	    PKG_FIND("libunicap" WITH_UNICAP UNICAP_CFLAGS UNICAP_LDLIBS)
	    IF(WITH_UNICAP)
		SET(AV_CFLAGS ${AV_CFLAGS} ${UNICAP_CFLAGS})
		SET(AV_LDLIBS ${AV_LDLIBS} ${UNICAP_LDLIBS})
	    ELSE(WITH_UNICAP)
		MESSAGE("** Unicap library not found.\n"
			"   Grabbing support via unicap will not be built.")
	    ENDIF(WITH_UNICAP)
	ELSE(WITH_UNICAP)
	    MESSAGE("** Unicap support disabled.\n"
		    "   Grabbing support via unicap will not be built.")
	ENDIF(WITH_UNICAP)

	# WITH_MVIMPACT
	IF(WITH_MVIMPACT)
	    FIND_PATH(MV_INCLUDE_DIR mvDeviceManager/Include/mvDeviceManager.h /usr/local/include)
	    FIND_LIBRARY(MV_LIBRARY NAMES mvDeviceManager PATHS /usr/local/lib)
	    IF(MV_INCLUDE_DIR AND MV_LIBRARY)
		SET(AV_INCLUDE_DIR ${AV_INCLUDE_DIR} ${MV_INCLUDE_DIR})
		SET(AV_LIBRARIES ${AV_LIBRARIES} ${MV_LIBRARY})
	        FIND_LIBRARY(MV_PROP NAMES mvPropHandling PATHS /usr/local/lib)
		IF(MV_PROP)
		    SET(AV_LIBRARIES ${AV_LIBRARIES} ${MV_PROP})
		ENDIF(MV_PROP)
		MESSAGE(STATUS "Found mvDeviceManager: ${MV_LIBRARY}")
	    ELSE(MV_INCLUDE_DIR AND MV_LIBRARY)
		SET(WITH_MVIMPACT 0)
		MESSAGE("** mvDeviceManager library not found.\n"
			"   Grabbing support via mvIMPACT Acquire will not be built.")
	    ENDIF(MV_INCLUDE_DIR AND MV_LIBRARY)
	    MARK_AS_ADVANCED(MV_INCLUDE_DIR MV_LIBRARY MV_PROP)
	ELSE(WITH_MVIMPACT)
	    MESSAGE("** mvDeviceManager support disabled.\n"
		    "   Grabbing support via mvIMPACT Acquire will not be built.")
	ENDIF(WITH_MVIMPACT)

	# WITH_UEYE
	IF(WITH_UEYE)
	    FIND_PATH(UEYE_INCLUDE_DIR ueye.h /usr/local/include)
	    FIND_LIBRARY(UEYE_LIBRARY NAMES ueye_api ueye_api64 PATHS /usr/local/lib)
	    IF(UEYE_INCLUDE_DIR AND UEYE_LIBRARY)
		SET(AV_INCLUDE_DIR ${AV_INCLUDE_DIR} ${UEYE_INCLUDE_DIR})
		SET(AV_LIBRARIES ${AV_LIBRARIES} ${UEYE_LIBRARY})
		MESSAGE(STATUS "Found uEye: ${UEYE_LIBRARY}")
	    ELSE(UEYE_INCLUDE_DIR AND UEYE_LIBRARY)
		SET(WITH_UEYE 0)
		MESSAGE("** uEye library not found.\n"
			"   Grabbing support via IDS uEye will not be built.")
	    ENDIF(UEYE_INCLUDE_DIR AND UEYE_LIBRARY)
	    MARK_AS_ADVANCED(UEYE_INCLUDE_DIR UEYE_LIBRARY)
	ELSE(WITH_UEYE)
	    MESSAGE("** uEye support disabled.\n"
		    "   Grabbing support via IDS uEye will not be built.")
	ENDIF(WITH_UEYE)

	# WITH_LIBV4L
	IF(WITH_LIBV4L)
	    PKG_FIND("libv4lconvert" WITH_LIBV4L LIBV4L_CFLAGS LIBV4L_LDLIBS)
	    IF(WITH_LIBV4L)
		SET(AV_CFLAGS ${AV_CFLAGS} ${LIBV4L_CFLAGS})
		SET(AV_LDLIBS ${AV_LDLIBS} ${LIBV4L_LDLIBS})
	    ELSE(WITH_LIBV4L)
		MESSAGE("** libv4lconvert library not found.\n"
			"   libv4lconvert will not be used in the v4l2 grabbing driver.")
	    ENDIF(WITH_LIBV4L)
	ELSE(WITH_LIBV4L)
	    MESSAGE("** libv4l support disabled.\n"
		    "   libv4lconvert will not be used in the v4l2 grabbing driver.")
	ENDIF(WITH_LIBV4L)
    ENDIF(NOT CMAKE_SYSTEM MATCHES "Linux*")
ELSE(WITH_GRABBER)
    MESSAGE("** GRABBER disabled.\n"
	    "   Grabbing support from a camera will not be built.")
ENDIF(WITH_GRABBER)

# WITH_DACS
FIND_PATH(DACS_INCLUDE_DIR dacs.h $ENV{D3}/include /usr/local/include)
FIND_LIBRARY(DACS_LIBRARIES NAMES dacs PATHS $ENV{D3}/lib /usr/local/lib)
IF(WITH_DACS AND DACS_INCLUDE_DIR AND DACS_LIBRARIES)
    IF(CMAKE_SYSTEM MATCHES "SunOS*")
	SET(DACS_LIBRARIES ${DACS_LIBRARIES} rt)
    ENDIF(CMAKE_SYSTEM MATCHES "SunOS*")
    ADD_DEFINITIONS(-DDRAFTC)
    MESSAGE(STATUS "Found DACS: ${DACS_LIBRARIES}")
ELSE(WITH_DACS AND DACS_INCLUDE_DIR AND DACS_LIBRARIES)
    IF(WITH_DACS)
	MESSAGE("** DACS library not found.\n"
	        "   Interprocess communication via DACS will not be built.")
    ENDIF(WITH_DACS)
    # Clear all variables which are used below
    SET(DACS_INCLUDE_DIR "")
    SET(DACS_LIBRARIES "")
    SET(WITH_DACS 0)
ENDIF(WITH_DACS AND DACS_INCLUDE_DIR AND DACS_LIBRARIES)
MARK_AS_ADVANCED(DACS_INCLUDE_DIR DACS_LIBRARIES)

# WITH_XCF
IF(WITH_XCF)
    PKG_FIND("xcf" WITH_XCF XCF_CFLAGS XCF_LDLIBS)
    IF(NOT WITH_XCF)
	MESSAGE("** XCF library not found.\n"
		"   XCF communication support will not be built.")
    ENDIF(NOT WITH_XCF)
ELSE(WITH_XCF)
    MESSAGE("** XCF support disabled.\n"
	    "   XCF communication support will not be built.")
ENDIF(WITH_XCF)

# WITH_READLINE
IF(WITH_READLINE)
    FIND_PATH(RLINE_INCLUDE_DIR readline/readline.h /usr/local/include)
    FIND_LIBRARY(RLINE_LIBRARIES NAMES readline PATHS /usr/local/lib)
    IF(RLINE_INCLUDE_DIR AND RLINE_LIBRARIES)
	MESSAGE(STATUS "Found readline: ${RLINE_LIBRARIES}")
    ELSE(RLINE_INCLUDE_DIR AND RLINE_LIBRARIES)
	SET(WITH_READLINE 0)
	MESSAGE("** readline library not found.\n"
		"   icewing-control will not be built.")
    ENDIF(RLINE_INCLUDE_DIR AND RLINE_LIBRARIES)
    MARK_AS_ADVANCED(RLINE_INCLUDE_DIR RLINE_LIBRARIES)
ELSE(WITH_READLINE)
    MESSAGE("** readline support disabled.\n"
	    "   icewing-control will not be built.")
ENDIF(WITH_READLINE)

###
### Subprojects
###

ADD_SUBDIRECTORY(utils)
ADD_SUBDIRECTORY(docs)

###
### Make "config.h"
###

CONFIGURE_FILE(${SOURCE_DIR}/config.h.in ${SOURCE_DIR}/config.h)

###
### Build iceWing
###

IF(NOT WITH_GRABBER)
    INCLUDE_DIRECTORIES(${SOURCE_DIR}/libs)
    SET(SOURCES ${SOURCES} libs/AVImg.c)
ENDIF(NOT WITH_GRABBER)

IF(NOT WITH_DACS)
    INCLUDE_DIRECTORIES(${SOURCE_DIR}/libs/dacs)
    SET(SOURCES ${SOURCES} libs/dacs/dacs.c)
ENDIF(NOT WITH_DACS)

ADD_EXECUTABLE(${PROGNAME} ${SOURCES})
SET_TARGET_PROPERTIES(${PROGNAME} PROPERTIES
    SKIP_BUILD_RPATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE)

ADD_DEFINITIONS(${GTK_CFLAGS} ${GDK_PIXBUF_CFLAGS} ${FFMPEG_CFLAGS} ${AV_CFLAGS})
INCLUDE_DIRECTORIES(
    ${JPEG_INCLUDE_DIR}
    ${PNG_INCLUDE_DIR}
    ${AV_INCLUDE_DIR}
    ${DACS_INCLUDE_DIR})
SET(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS}")

TARGET_LINK_LIBRARIES(${PROGNAME}
    ${JPEG_LIBRARIES}
    ${PNG_LIBRARIES}
    ${AV_LIBRARIES}
    ${DACS_LIBRARIES}
    ${GTK_LDLIBS} 
    ${GDK_PIXBUF_LDLIBS} 
    ${FFMPEG_LDLIBS} 
    ${AV_LDLIBS}
    ${X11_LIBRARIES}
    dl
)

# General flags
INCLUDE_DIRECTORIES(${SOURCE_DIR} ${SOURCE_DIR}/plugins)

# Set C_FLAGS
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_FLAGS}")

###
### Make "icewing-config"
###

SET(libs "")
IF(WITH_DACS)
    SET(cflags "-I${INCLUDEDIR} -I${DACS_INCLUDE_DIR} ${GTK_CFLAGS}")
ELSE(WITH_DACS)
    SET(cflags "-I${INCLUDEDIR} -I${INCLUDEDIR}/dacs ${GTK_CFLAGS}")
ENDIF(WITH_DACS)
SET(PINCLUDEDIR "${INCLUDEDIR_B}/iwPlugins")
EXECUTE_PROCESS(COMMAND grep "VERSION[[:space:]]*=" ${SOURCE_DIR}/main/icewing.c
    COMMAND sed "s/[^0-9]*\"\\([^\"]*\\)\".*/\\1/"
    OUTPUT_VARIABLE VERSION)
STRING(REGEX REPLACE "\n" "" VERSION "${VERSION}")
CONFIGURE_FILE(${SOURCE_DIR}/icewing-config.in ${SOURCE_DIR}/icewing-config @ONLY)
CONFIGURE_FILE(${SOURCE_DIR}/icewing.pc.in ${SOURCE_DIR}/icewing.pc @ONLY)

###
### Installation rules
###

# Header files
SET(INCL_GUI 
    gui/Gcolor.h 
    gui/Gdata.h 
    gui/Gdialog.h 
    gui/Ggui.h
    gui/Gimage.h
    gui/Goptions.h 
    gui/Gpolygon.h 
    gui/Gpreview.h 
    gui/Grender.h 
    gui/Gsession.h)
SET(INCL_MAIN 
    main/grab.h
    main/grab_prop.h
    main/image.h 
    main/output.h 
    main/plugin.h
    main/region.h 
    main/plugin_comm.h 
    main/plugin_cxx.h 
    main/sfb_iw.h)
SET(INCL_TOOL 
    tools/tools.h 
    tools/opencv.h)
SET(INCL_DACS 
    libs/dacs/dacs.h)
SET(INCL_ICEMM
    icemm/PluginData.hpp
)

# Data files
SET(DATA_TMPL
    data/tmpl/Makefile
    data/tmpl/TMPL.c
    data/tmpl/TMPL.cpp
    data/tmpl/TMPL.h)

# bin - directory
INSTALL(TARGETS ${PROGNAME}
    DESTINATION ${BINDIR})
INSTALL(PROGRAMS icewing-config
    DESTINATION ${BINDIR})

# include - directory
INSTALL(FILES ${INCL_GUI}
    DESTINATION ${INCLUDEDIR}/gui)
INSTALL(FILES ${INCL_MAIN}
    DESTINATION ${INCLUDEDIR}/main)
INSTALL(FILES ${INCL_TOOL}
    DESTINATION ${INCLUDEDIR}/tools)
IF(NOT WITH_DACS)
    INSTALL(FILES ${INCL_DACS}
	DESTINATION ${INCLUDEDIR}/dacs)
ENDIF(NOT WITH_DACS)
INSTALL(FILES ${INCL_ICEMM}
    DESTINATION ${INCLUDEDIR}/icemm)
INSTALL(DIRECTORY DESTINATION "${INCLUDEDIR_B}/iwPlugins")

# lib - directory
INSTALL(DIRECTORY DESTINATION "${LIBDIR}")
INSTALL(FILES icewing.pc
    DESTINATION ${LIBDIR_B}/pkgconfig)

# man - directory
INSTALL(FILES icewing.1
    DESTINATION ${MANDIR}/man1)

# share - directory
INSTALL(FILES ${DATA_TMPL}
    DESTINATION ${DATADIR}/tmpl)

###
### Maintainance
###

# Doxygen config file
SET(DOXYGEN_FILES docs/icewing.dox ${INCL_GUI} ${INCL_TOOL} ${INCL_MAIN} ${INCL_ICEMM})
LIST(REMOVE_ITEM DOXYGEN_FILES main/sfb_iw.h)
CONFIGURE_FILE(${SOURCE_DIR}/Doxyfile.in ${SOURCE_DIR}/Doxyfile)

# Package and distribution
FILE(GLOB FILES RELATIVE ${SOURCE_DIR} COPYING README NEWS TODO HACKING Makefile icewing.1
    *.in CMakeLists.txt icewing-api-update
    cmake/cmake.txt
    utils/Makefile utils/*doxygen utils/*txt utils/*cpp utils/*in utils/scripts/*[hk]
    libs/*.[hc] libs/*/*.[hc]
    avvideo/*.[hc] gui/*.[hc] gui/avi/*.[hc] gui/images/*.xpm gui/images/*.png
    tools/*[hcp] main/*.[hc] main/*.cpp main/*.xpm main/*.xbm
    plugins/README plugins/Makefile plugins/*/Makefile plugins/*/*.[hc]
    plugins/*/*.cpp data/tmpl/*[ecph]
    icemm/*.[ch]pp
    docs/icewing.dox)
FILE(GLOB FDOCS RELATIVE ${SOURCE_DIR} docs/Makefile docs/CMakeLists.txt
    docs/*.bib docs/*.sty docs/*.ist docs/*.tex
    docs/tex/*.tex docs/images/*.png docs/images/*.fig)

ADD_CUSTOM_TARGET(pack
    tar --use-compress-program bzip2 -cvf icewing.tar.bz2 ${FILES} ${FDOCS}
    WORKING_DIRECTORY ${SOURCE_DIR})
ADD_CUSTOM_TARGET(dist
    sed \"s/n%s V%s \([A-Za-z0-9 ]*\),/n%s V%s \(`date +'%B %e %Y'|sed 's/  / /g'`\),/\" ../main/icewing.c >temp.1
    COMMAND mv temp.1 main/icewing.c
    COMMAND mkdir icewing-${VERSION}
    COMMAND \(cd icewing-${VERSION}\; tar --use-compress-program bzip2 -xvf ../icewing.tar.bz2\)
    COMMAND cp docs/icewing.pdf docs/quicktour.avi icewing-${VERSION}/docs
    COMMAND cp main/icewing.c icewing-${VERSION}/main
    COMMAND tar --use-compress-program bzip2 -cvf icewing-${VERSION}.tar.bz2 icewing-${VERSION}
    COMMAND rm -rf icewing-${VERSION}
    DEPENDS pack
    WORKING_DIRECTORY ${SOURCE_DIR})

MESSAGE("\n"
    "${PACKET} successfully configurated!\n"
    "\n"
    "'make' will now compile ${PACKET} and 'make install' will install it\n"
    "under '${PREFIX}'. 'make depend all install' in the directory\n"
    "    ${SOURCE_DIR}/plugins\n"
    "will additionally install all external plugins (AFTER the global make install).\n"
    "\n"
    "If single libraries could not be found (see lines starting with '**' for any\n"
    "problems), you can specify with the environment variables 'CMAKE_INCLUDE_PATH'\n"
    "and 'CMAKE_LIBRARY_PATH' where cmake should search for them.\n")
