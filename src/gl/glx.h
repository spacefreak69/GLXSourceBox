#ifndef SB_GLX_H
#define SB_GLX_H

// simple header to make glew not link with x11 on linux, so wayland is compatible

// disable glew's glu, instead use libglu or the system's glu
#ifndef GLEW_NO_GLU
#define GLEW_NO_GLU 1
#endif

#include <SDL.h>

#include <GL/glew.h>

#endif /* SB_GLX_H */
