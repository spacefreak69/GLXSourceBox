#ifndef SB_GLG_GLU_H
#define SB_GLG_GLU_H

// define the 7 functions i use as GLAPIENTRY so mesa doesnt try to pull from non existant files on the windows build
// externed to "C" because libglu has some cpp and i dont want it trying to include this file somehow

#include "gl/glx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GLUquadric GLUquadric;

// copy paste from libglu mesa header
#define GLU_FLAT    100000
#define GLU_SMOOTH  100001
#define GLU_NONE    100002
#define GLU_SILENT  100003
#define GLU_OUTLINE 100004

GLUquadric *GLAPIENTRY gluNewQuadric(void);
void GLAPIENTRY gluDeleteQuadric(GLUquadric *quadric);
void GLAPIENTRY gluQuadricNormals(GLUquadric *quadric, GLenum style);
void GLAPIENTRY gluSphere(GLUquadric *quadric, GLdouble radius, GLint slices, GLint stacks);
void GLAPIENTRY gluCylinder(GLUquadric *quadric, GLdouble baseRadius, GLdouble topRadius,
                            GLdouble height, GLint slices, GLint stacks);

void GLAPIENTRY gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);

GLint GLAPIENTRY gluUnProject(GLdouble objX, GLdouble objY, GLdouble objZ,
                              const GLdouble modelMatrix[16], const GLdouble projMatrix[16],
                              const GLint viewport[4],
                              GLdouble *winX, GLdouble *winY, GLdouble *winZ);

#ifdef __cplusplus
}
#endif

#endif /* SB_GLG_GLU_H */