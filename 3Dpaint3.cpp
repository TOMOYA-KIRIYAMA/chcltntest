/*------------------------------------------------------------------
	hiro �}�[�J��ɃL���[�u���ӂ�ӂ킳����
	8th
------------------------------------------------------------------*/

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#ifndef __APPLE__
#include <GL/gl.h>
#include <GL/glut.h>
#else
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#endif
#include <AR/gsub.h>
#include <AR/video.h>
#include <AR/param.h>
#include <AR/ar.h>
#include <GL/glpng.h>
#include <math.h> // cos���g�p

#ifdef _DEBUG
	#pragma comment(lib,"libARd.lib")
	#pragma comment(lib,"libARgsubd.lib")
	#pragma comment(lib,"libARvideod.lib")
	#pragma comment(linker,"/NODEFAULTLIB:libcmtd.lib")
#else
	#pragma comment(lib,"libAR.lib")
	#pragma comment(lib,"libARgsub.lib")
	#pragma comment(lib,"libARvideo.lib")
	#pragma comment(linker,"/NODEFAULTLIB:libcmt.lib")
#endif

//
// Camera configuration.
//
#ifdef _WIN32
char			*vconf = "Data\\WDM_camera_flipV.xml";
#else
char			*vconf = "";
#endif

#define PTT_NUM				2

#define PTT1_MARK_ID		0					// �p�^�[�� .hiro
#define PTT1_PATT_NAME		"Data/patt.hiro"
#define PTT1_SIZE			80.0

#define PTT2_MARK_ID		1					// �p�^�[�� .kanji
#define PTT2_PATT_NAME		"Data/patt.kanji"
#define PTT2_SIZE			80.0

#define CUBE_SIZE 15.0

int             xsize, ysize;
int             thresh = 100;
int             count = 0;
int				isFirst[PTT_NUM];	// �o�͂̂Ԃ�C���p
double			itrans2[3][4];		// �}�[�J2�̋t�s��

float c_angle[3] = { 0.0, 0.0, 0.0 };
float c_trans[3] = { 0.0, 0.0, 50.0};
pngInfo info;
GLuint texture;
char *texname = "Data/sample0.png";

char           *cparam_name    = "Data/camera_para.dat";
ARParam        cparam;

typedef struct{
	char			*patt_name;				//�p�^�[���t�@�C����
	int             patt_id;				//�p�^�[��ID
	int				mark_id;				//�}�[�JID
	int				visible;				//���o
	double          patt_width;				//�p�^�[���̕�
	double          patt_center[2];			//�p�^�[���̒��S���W
	double          patt_trans[3][4];		//���W�ϊ��s��
}PTT_T;

PTT_T object[PTT_NUM] = {
	{PTT1_PATT_NAME, -1, PTT1_MARK_ID, 0, PTT1_SIZE, {0.0,0.0}},
	{PTT2_PATT_NAME, -1, PTT2_MARK_ID, 0, PTT2_SIZE, {0.0,0.0}}
};

static void   init(void);
static void   cleanup(void);
static void   keyEvent( unsigned char key, int x, int y);
static void   mainLoop(void);
static void   draw( void );
void mySetLight( void );
void mySetMaterial( GLfloat R, GLfloat G, GLfloat B);

void myMatrix(double trans[3][4]);
void print_how_to_use(void);
void print_programmer(void);

void mySelectWire(int i);
void mySelectColor(GLfloat R, GLfloat G, GLfloat B);
void myMarkSquare (int patt_id);
void myCopyTrans (double s[3][4], double d[3][4]);

int isAppearPatt(int patt_id);
int isDisappearPatt(int patt_id);
void roundAppearPatt(int appv[PTT_NUM]);
void roundDisappearPatt(int disv[PTT_NUM]);

void Initialize(void);

int main(int argc, char **argv)
{
	print_programmer();
	glutInit(&argc, argv);
	init();
	Initialize(); // glpng �̏���

    arVideoCapStart();
	print_how_to_use();
    argMainLoop( NULL, keyEvent, mainLoop );
	return (0);
}


void print_programmer(void){
	printf("Programmer : Tomoya Kiriyama\n\n");
}


void print_how_to_use(void){
	printf("\t�y3D paint�z\n");
	printf("�g�p�}�[�J�F patt.hiro	patt.kanji\n");
	printf("\thiro �ŊG��`���Ckanji �ō��W�ϊ�\n");
	printf("patt.hiro  : �����͂��߂̓_����C�����Ȃ��Ȃ����_�܂Ő�������\n"
		"patt.kanji : ���R�ɍ��W�ϊ����s����\n"
		"           : �`���������ɃA�j���[�V������t���ĕ\��\n");
	printf("\nCommand\n"
		"Esc : �I��\n"
		" a  : �A�j���[�V�����̕ύX\n"
		" s  : ���W���̕\��\n"
		" u  : ��{�d�l�̍ĕ\��\n"
		" h  : kanji �̃}�[�J�̐Ղ̕\��/��\��\n"
		" p  : ���Wlog�� �o��\n");
}


static void   keyEvent( unsigned char key, int x, int y)
{
    /* quit if the ESC key is pressed */
    if( key == 0x1b ) {
        printf("*** %f (frame/sec)\n", (double)count/arUtilTimer());
        cleanup();
        exit(0);
    }
}


/* main loop */
static void mainLoop(void)
{
    ARUint8         *dataPtr;
    ARMarkerInfo    *marker_info;
    int             marker_num;
    int             j, k;
	int				i;

    /* grab a vide frame */
    if( (dataPtr = (ARUint8 *)arVideoGetImage()) == NULL ) {
        arUtilSleep(2);
        return;
    }
    if( count == 0 ) arUtilTimerReset();
    count++;

    argDrawMode2D();
    argDispImage( dataPtr, 0,0 );

    /* detect the markers in the video frame */
    if( arDetectMarker(dataPtr, thresh, &marker_info, &marker_num) < 0 ) {
        cleanup();
        exit(0);
    }

    arVideoCapNext();

    /* check for object visibility */
   	for( i = 0; i < PTT_NUM; i++){
		k = -1;
	    for( j = 0; j < marker_num; j++ ) {
	        if( object[i].patt_id == marker_info[j].id ) {
	            if( k == -1 ) k = j;
	            else if( marker_info[k].cf < marker_info[j].cf ) k = j;
	        }	
	    }
		if( k == -1 ) { /* not find marker */
			object[i].visible = 0;
			//isFirst[i] = 1;
		}
		else{
			/* get the transformation between the marker and the real camera */
			if( isFirst[i]){
				arGetTransMat(&marker_info[k], object[i].patt_center, object[i].patt_width, object[i].patt_trans);
			}else{
				arGetTransMatCont(&marker_info[k], object[i].patt_trans, object[i].patt_center, object[i].patt_width, object[i].patt_trans);
			}
			object[i].visible = 1;
			//isFirst[i] = 0;
			
			/* �ǉ� */
			if(i == PTT2_MARK_ID){
				arUtilMatInv( object[PTT2_MARK_ID].patt_trans, itrans2); // �t�s��̌v�Z
			}
		}
	}

	//Initialize(); // fix me
	draw();

	argSwapBuffers();
}


static void init( void )
{
    ARParam  wparam;
	int i;
	
    /* open the video path */
    if( arVideoOpen( vconf ) < 0 ) exit(0);
    /* find the size of the window */
    if( arVideoInqSize(&xsize, &ysize) < 0 ) exit(0);
    printf("Image size (x,y) = (%d,%d)\n", xsize, ysize);

    /* set the initial camera parameters */
    if( arParamLoad(cparam_name, 1, &wparam) < 0 ) {
        printf("Camera parameter load error !!\n");
        exit(0);
    }
    arParamChangeSize( &wparam, xsize, ysize, &cparam );
    arInitCparam( &cparam );
    printf("*** Camera Parameter ***\n");
    arParamDisp( &cparam );

	for( i=0; i < PTT_NUM; i++){ 
		if( (object[i].patt_id = arLoadPatt(object[i].patt_name)) < 0 ) {
			printf("�p�^�[���ǂݍ��݂Ɏ��s���܂���!! %s\n", object[i].patt_name);
			exit(0);
		}
		isFirst[i] = 1;
	}

    /* open the graphics window */
    argInit( &cparam, 1.0, 0, 0, 0, 0 );

}


/* cleanup function called when program exits */
static void cleanup(void)
{
    arVideoCapStop();
    arVideoClose();
    argCleanup();
}


//�s��ϊ��̓K�p
void myMatrix(double trans[3][4]){
	double gl_para[16];

	argConvGlpara( trans, gl_para);
	glMatrixMode( GL_MODELVIEW);
	glLoadMatrixd( gl_para);
}



void test(void){
	static float color[3] = {1.0, 0.0, 0.0}; // for debug
	float tmp_c;					     // for debug

	if( isAppearPatt(PTT1_MARK_ID)){
		tmp_c = color[0];
		color[0] = color[1];
		color[1] = color[2];
		color[2] = tmp_c;

		mySelectColor(color[0], color[1], color[2]);
	}

	if( isDisappearPatt(PTT1_MARK_ID)){
		mySelectColor(color[2], color[0], color[1]);
	}

}


void test2(void){
	static float color[3] = {0.0, 0.0, 0.0}; // for debug
	float tmp_c;					     // for debug
	int vis[PTT_NUM];
	int dis[PTT_NUM];

	roundAppearPatt(vis);
	roundDisappearPatt(dis);

	if(vis[PTT1_MARK_ID]){
		color[0] = 1.0 - color[0]; // red
	}
	if(vis[PTT2_MARK_ID]){
		color[1] = 1.0 - color[1]; // green
	}
	if(dis[PTT2_MARK_ID]){
		color[2] = 1.0 - color[2]; // blue
	}

	mySelectColor(color[0], color[1], color[2]);
		
}


/* �Ăяo����邽�тɉ�]����cos�̒l */
double renew_cos(void)
{
#define PI 3.141592
	static double theta = 0.0;
	double omega = 5.0;
	double rad = theta * PI / 180.0;

	theta += omega;
	return cos(rad);
}


/* �t���[�����ƂɍX�V������e */
void renew(void)
{
#define Z_TRANS_BASE 60.0
	double range = 40.0; // cos�g�`�̐U�ꕝ
	c_angle[0] += 5.0; // x_angle
	//c_angle[1] += 5.0; // y_angle
	c_angle[2] += 5.0; // z_angle
	
	c_trans[2] = range* renew_cos() + Z_TRANS_BASE;
}


static void draw(void) {
	int i,j,k;
	float size = CUBE_SIZE;
	float normals[6][3] = {
		{ 0.0,  0.0, -1.0},
		{ 1.0,  0.0,  0.0},
		{ 0.0,  0.0,  1.0},
		{-1.0,  0.0,  0.0},
		{ 0.0,  1.0,  0.0},
		{ 0.0, -1.0,  0.0}
	};
		
	float cube_points[8][3] ={
		{ -size, -size, -size },
		{  size, -size, -size },
		{  size, -size,  size },
		{ -size, -size,  size },
		{ -size,  size, -size },
		{  size,  size, -size },
		{  size,  size,  size },
		{ -size,  size,  size }
	};

	/* 3D�I�u�W�F�N�g��`�悷�邽�߂̏��� */
	argDrawMode3D();
	argDraw3dCamera(0, 0);


	glClear(GL_DEPTH_BUFFER_BIT); //�o�b�t�@�̏���
	glEnable( GL_DEPTH_TEST );		// �A�ʏ����̓K�p

	mySetLight();
	glEnable( GL_LIGHTING );

	myMatrix(object[PTT1_MARK_ID].patt_trans);
	
	renew(); // �t���[�����̍X�V���e

	// ������
	glPushMatrix();
		glTranslated( c_trans[0], c_trans[1], c_trans[2]);//���s�ړ��l�̐ݒ�
		glRotatef(c_angle[0], 1.0, 0.0, 0.0 );
		glRotatef(c_angle[1], 0.0, 1.0, 0.0 );
		glRotatef(c_angle[2], 0.0, 0.0, 1.0 );
		
		/* �O */
		glEnable( GL_TEXTURE_2D );
		glBegin( GL_QUADS );
			mySetMaterial( 1.0, 1.0, 1.0 );
			glBindTexture(GL_TEXTURE_2D, texture);
			glNormal3fv(normals[0]);
			glTexCoord2f(0.0, 1.0); glVertex3fv(cube_points[0]);
			glTexCoord2f(1.0, 1.0); glVertex3fv(cube_points[1]);
			glTexCoord2f(1.0, 0.0); glVertex3fv(cube_points[5]);
			glTexCoord2f(0.0, 0.0); glVertex3fv(cube_points[4]);
		glEnd();
		glDisable( GL_TEXTURE_2D );
		
		/* �E */
		glBegin( GL_QUADS );
			mySetMaterial( 0.0, 1.0, 0.0);
			glNormal3fv(normals[1]);
			glVertex3fv(cube_points[1]);
			glVertex3fv(cube_points[2]);
			glVertex3fv(cube_points[6]);
			glVertex3fv(cube_points[5]);
		glEnd();

		/* ��� */
		glBegin( GL_QUADS );
			mySetMaterial( 0.0, 1.0, 1.0);
			glNormal3fv(normals[2]);
			glVertex3fv(cube_points[2]);
			glVertex3fv(cube_points[3]);
			glVertex3fv(cube_points[7]);
			glVertex3fv(cube_points[6]);
		glEnd();

		/* �� */
		glBegin( GL_QUADS );
			mySetMaterial( 1.0, 0.0, 1.0);
			glNormal3fv(normals[3]);
			glVertex3fv(cube_points[3]);
			glVertex3fv(cube_points[0]);
			glVertex3fv(cube_points[4]);
			glVertex3fv(cube_points[7]);
		glEnd();

		/* �� */
		glBegin( GL_QUADS );
			mySetMaterial( 0.0, 0.0, 1.0);
			glNormal3fv(normals[4]);
			glVertex3fv(cube_points[4]);
			glVertex3fv(cube_points[5]);
			glVertex3fv(cube_points[6]);
			glVertex3fv(cube_points[7]);
		glEnd();

		/* �� */
		glBegin( GL_QUADS );
			mySetMaterial( 1.0, 1.0, 0.0);
			glNormal3fv(normals[5]);
			glVertex3fv(cube_points[0]);
			glVertex3fv(cube_points[1]);
			glVertex3fv(cube_points[2]);
			glVertex3fv(cube_points[3]);
		glEnd();

	glPopMatrix();


	glDisable( GL_LIGHTING );
	glDisable( GL_DEPTH_TEST );		// �A�ʏ����̓K�p
}


static void draw2( void )
{
	static int before_visible[2] = {0, 0};
	static int Wire_count = -1;  // �o�͂��� Wire �̑I��(0�`4)
	static int flag_first1 = 0;  // ��x�ł� hiro �����������痧��

	static double begin[3];
		
	/* 3D�I�u�W�F�N�g��`�悷�邽�߂̏��� */
	argDrawMode3D();
	argDraw3dCamera(0, 0);

	/* Z�o�b�t�@�Ȃǂ̐ݒ� */
	glClear( GL_DEPTH_BUFFER_BIT );	// �A�ʏ����̐ݒ�
	glEnable( GL_DEPTH_TEST );		// �A�ʏ����̓K�p

	mySetLight();				// �����̐ݒ�
	glEnable( GL_LIGHTING );	// �����̓K�p

	test2();
	

	myMarkSquare(PTT1_MARK_ID); // ����m�F�p�R�[�h

		/***************************/
		/*                         */
		/*       draw�֐�������    */
		/*                         */
		/*                         */
		/***************************/






	glDisable( GL_LIGHTING );	// �����̖�����
	glDisable( GL_DEPTH_TEST );	// �A�ʏ����̖�����
}


// �����̐ݒ�
void mySetLight( void ){
	GLfloat light_diffuse[]  = { 0.9, 0.9, 0.9, 1.0 }; // �g�U���ˌ�
	GLfloat light_specular[] = { 1.0, 1.0, 1.0, 1.0 }; // ���ʔ��ˌ�
	GLfloat light_ambient[]  = { 0.3, 0.3, 0.3, 0.1 }; // ����
	GLfloat light_position[] = { 0.0, 100.0, 50.0, 1.0 }; // �ʒu�Ǝ��

	/* �����̐ݒ� */
	glLightfv( GL_LIGHT0, GL_DIFFUSE, light_diffuse );
	glLightfv( GL_LIGHT0, GL_SPECULAR, light_specular );
	glLightfv( GL_LIGHT0, GL_AMBIENT, light_ambient );
	glLightfv( GL_LIGHT0, GL_POSITION, light_position );

	glShadeModel( GL_SMOOTH ); // �V�F�[�f�B���O�̎�ނ̐ݒ�
	glEnable( GL_LIGHT0 ); // �����̗L����
}


void mySetMaterial( GLfloat R, GLfloat G, GLfloat B){
	GLfloat mat_diffuse[]  = { R, G, B, 1.0 };	// �g�U���̔��ˌW��
	GLfloat mat_specular[] = { R, G, B, 1.0 };	// ���ʌ��̔��ˌW��
	GLfloat mat_ambient[]  = { 0.3, 0.3, 0.3, 1.0 };	// �����̔��ˌW��
	GLfloat shininess[]    = { 50.0 };					// ���ʌ��̎w��

	/* �ގ����� */
	glMaterialfv( GL_FRONT, GL_DIFFUSE, mat_diffuse );		// �g�U���˂̐ݒ�
	glMaterialfv( GL_FRONT, GL_SPECULAR, mat_specular );	// ���ʔ��˂̐ݒ�
	glMaterialfv( GL_FRONT, GL_AMBIENT, mat_ambient );		// �����˂̐ݒ�
	glMaterialfv( GL_FRONT, GL_SHININESS, shininess );		// ���ʔ��˂̉s���̐ݒ�
}


// 3D�I�u�W�F�N�g��`��(�����ɂ���ĕω�)
void mySelectWire(int i){
	switch(i){
		case 0:
			glutWireSphere( 30.0, 10, 10);
			break;
		case 1:
			glutWireCube( 40.0 );
			break;
		case 2:
			glScalef( 1.5, 1.5, 1.5 );
			glutWireTorus( 10.0, 20.0, 20, 6);
			break;
		case 3:
			glScalef( 40.0, 40.0, 40.0 );
			glutWireTetrahedron();
			break;
		case 4:
			glTranslatef( 0.0, 0.0, 10.0 );
			glRotatef( 90.0, 1.0, 0.0, 0.0 );
			glutWireTeapot( 40.0 );
			break;
		default:
			glutWireIcosahedron();
			printf("EROOR: Wire_count\n");
			break;
	}
}


// �����ɂ���ĐF��ω�������
void mySelectColor(GLfloat R, GLfloat G, GLfloat B){
	mySetMaterial( R, G, B);
}


/* �}�[�J������鐳���`��`�悷��֐� */
/* ���̐F�͐ݒ肵�Ă��Ȃ�             */
void myMarkSquare(int patt_id){
	glPushMatrix();
	myMatrix(object[patt_id].patt_trans);
		glLineWidth(3.0);             // ���̑�����ݒ�
		glBegin( GL_LINE_LOOP );
			glVertex3f( -object[patt_id].patt_width/2.0, -object[patt_id].patt_width/2.0, 0.0 );
			glVertex3f(  object[patt_id].patt_width/2.0, -object[patt_id].patt_width/2.0, 0.0 );
			glVertex3f(  object[patt_id].patt_width/2.0,  object[patt_id].patt_width/2.0, 0.0 );
			glVertex3f( -object[patt_id].patt_width/2.0,  object[patt_id].patt_width/2.0, 0.0 );
		glEnd();
	glPopMatrix();
}


// �s��� s ���� d �ɃR�s�[����
void myCopyTrans (double s[3][4], double d[3][4]){
	int i, k;

	for(i=0; i < 3; i++){
		for(k = 0; k < 4; k++){
			d[i][k] = s[i][k];
		}
	}
}


/* �C�ӂ�patt���o�������� */
int isAppearPatt(int patt_id){
	static int before[PTT_NUM]; // 1�O�̃t���[���ł̊e�p�b�g��visible�l 
	static int isFirst = 1; // ���̊֐������߂Ďg�p�����Ȃ�1
	int i;
	int v = 0;
	
	if (isFirst){ // before �̏�����
		for (i=0; i<PTT_NUM; i++){
			before[i] = 0;
		}
		isFirst = 0;
	}

	/* (before) invisible & (now) visible */
	if ( !before[patt_id] && object[patt_id].visible){
		v = 1;
	}
	before[patt_id] = object[patt_id].visible;

	return v;
}


/* �C�ӂ�patt������������ */
int isDisappearPatt(int patt_id){
	static int before[PTT_NUM]; // 1�O�̃t���[���ł̊e�p�b�g��visible�l 
	static int isFirst = 1; // ���̊֐������߂Ďg�p�����Ȃ�1
	int i;
	int v = 0;
	
	if (isFirst){ // before �̏�����
		for (i=0; i<PTT_NUM; i++){
			before[i] = 0;
		}
		isFirst = 0;
	}

	/* (before) visible & (now) invisible */
	if ( before[patt_id] && !object[patt_id].visible){
		v = 1;
	}
	before[patt_id] = object[patt_id].visible;

	return v;

}


/* ���ׂĂ�patt�ɂ��ďo������   */
/* ����appv�ɂ��ꂼ��̌��ʂ��i�[ */
void roundAppearPatt(int appv[PTT_NUM]){
	int i;
	
	for (i=0; i<PTT_NUM; i++){
		appv[i] = isAppearPatt(i);
	}
}


/* ���ׂĂ�patt�ɂ��ď�������   */
/* ����appv�ɂ��ꂼ��̌��ʂ��i�[ */
void roundDisappearPatt(int disv[PTT_NUM]){
	int i;
	
	for (i=0; i<PTT_NUM; i++){
		disv[i] = isDisappearPatt(i);
	}
}

void Initialize(void){
	texture = pngBind ( texname, PNG_NOMIPMAP, PNG_ALPHA, &info, GL_CLAMP, GL_NEAREST, GL_NEAREST);
}