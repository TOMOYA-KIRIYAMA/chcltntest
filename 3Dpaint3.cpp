/*------------------------------------------------------------------
	hiro マーカ上にキューブをふわふわさせる
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
#include <math.h> // cosを使用

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

#define PTT1_MARK_ID		0					// パターン .hiro
#define PTT1_PATT_NAME		"Data/patt.hiro"
#define PTT1_SIZE			80.0

#define PTT2_MARK_ID		1					// パターン .kanji
#define PTT2_PATT_NAME		"Data/patt.kanji"
#define PTT2_SIZE			80.0

#define CUBE_SIZE 15.0

int             xsize, ysize;
int             thresh = 100;
int             count = 0;
int				isFirst[PTT_NUM];	// 出力のぶれ修正用
double			itrans2[3][4];		// マーカ2の逆行列

float c_angle[3] = { 0.0, 0.0, 0.0 };
float c_trans[3] = { 0.0, 0.0, 50.0};
pngInfo info;
GLuint texture;
char *texname = "Data/sample0.png";

char           *cparam_name    = "Data/camera_para.dat";
ARParam        cparam;

typedef struct{
	char			*patt_name;				//パターンファイル名
	int             patt_id;				//パターンID
	int				mark_id;				//マーカID
	int				visible;				//検出
	double          patt_width;				//パターンの幅
	double          patt_center[2];			//パターンの中心座標
	double          patt_trans[3][4];		//座標変換行列
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
	Initialize(); // glpng の準備

    arVideoCapStart();
	print_how_to_use();
    argMainLoop( NULL, keyEvent, mainLoop );
	return (0);
}


void print_programmer(void){
	printf("Programmer : Tomoya Kiriyama\n\n");
}


void print_how_to_use(void){
	printf("\t【3D paint】\n");
	printf("使用マーカ： patt.hiro	patt.kanji\n");
	printf("\thiro で絵を描き，kanji で座標変換\n");
	printf("patt.hiro  : 見えはじめの点から，見えなくなった点まで線を引く\n"
		"patt.kanji : 自由に座標変換を行える\n"
		"           : 描いた線分にアニメーションを付けて表示\n");
	printf("\nCommand\n"
		"Esc : 終了\n"
		" a  : アニメーションの変更\n"
		" s  : 座標情報の表示\n"
		" u  : 基本仕様の再表示\n"
		" h  : kanji のマーカの跡の表示/非表示\n"
		" p  : 座標logの 出力\n");
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
			
			/* 追加 */
			if(i == PTT2_MARK_ID){
				arUtilMatInv( object[PTT2_MARK_ID].patt_trans, itrans2); // 逆行列の計算
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
			printf("パターン読み込みに失敗しました!! %s\n", object[i].patt_name);
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


//行列変換の適用
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


/* 呼び出されるたびに回転するcosの値 */
double renew_cos(void)
{
#define PI 3.141592
	static double theta = 0.0;
	double omega = 5.0;
	double rad = theta * PI / 180.0;

	theta += omega;
	return cos(rad);
}


/* フレームごとに更新する内容 */
void renew(void)
{
#define Z_TRANS_BASE 60.0
	double range = 40.0; // cos波形の振れ幅
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

	/* 3Dオブジェクトを描画するための準備 */
	argDrawMode3D();
	argDraw3dCamera(0, 0);


	glClear(GL_DEPTH_BUFFER_BIT); //バッファの消去
	glEnable( GL_DEPTH_TEST );		// 陰面処理の適用

	mySetLight();
	glEnable( GL_LIGHTING );

	myMatrix(object[PTT1_MARK_ID].patt_trans);
	
	renew(); // フレーム毎の更新内容

	// 立方体
	glPushMatrix();
		glTranslated( c_trans[0], c_trans[1], c_trans[2]);//平行移動値の設定
		glRotatef(c_angle[0], 1.0, 0.0, 0.0 );
		glRotatef(c_angle[1], 0.0, 1.0, 0.0 );
		glRotatef(c_angle[2], 0.0, 0.0, 1.0 );
		
		/* 前 */
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
		
		/* 右 */
		glBegin( GL_QUADS );
			mySetMaterial( 0.0, 1.0, 0.0);
			glNormal3fv(normals[1]);
			glVertex3fv(cube_points[1]);
			glVertex3fv(cube_points[2]);
			glVertex3fv(cube_points[6]);
			glVertex3fv(cube_points[5]);
		glEnd();

		/* 後ろ */
		glBegin( GL_QUADS );
			mySetMaterial( 0.0, 1.0, 1.0);
			glNormal3fv(normals[2]);
			glVertex3fv(cube_points[2]);
			glVertex3fv(cube_points[3]);
			glVertex3fv(cube_points[7]);
			glVertex3fv(cube_points[6]);
		glEnd();

		/* 左 */
		glBegin( GL_QUADS );
			mySetMaterial( 1.0, 0.0, 1.0);
			glNormal3fv(normals[3]);
			glVertex3fv(cube_points[3]);
			glVertex3fv(cube_points[0]);
			glVertex3fv(cube_points[4]);
			glVertex3fv(cube_points[7]);
		glEnd();

		/* 上 */
		glBegin( GL_QUADS );
			mySetMaterial( 0.0, 0.0, 1.0);
			glNormal3fv(normals[4]);
			glVertex3fv(cube_points[4]);
			glVertex3fv(cube_points[5]);
			glVertex3fv(cube_points[6]);
			glVertex3fv(cube_points[7]);
		glEnd();

		/* 下 */
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
	glDisable( GL_DEPTH_TEST );		// 陰面処理の適用
}


static void draw2( void )
{
	static int before_visible[2] = {0, 0};
	static int Wire_count = -1;  // 出力する Wire の選択(0～4)
	static int flag_first1 = 0;  // 一度でも hiro が見つかったら立つ

	static double begin[3];
		
	/* 3Dオブジェクトを描画するための準備 */
	argDrawMode3D();
	argDraw3dCamera(0, 0);

	/* Zバッファなどの設定 */
	glClear( GL_DEPTH_BUFFER_BIT );	// 陰面処理の設定
	glEnable( GL_DEPTH_TEST );		// 陰面処理の適用

	mySetLight();				// 光源の設定
	glEnable( GL_LIGHTING );	// 光源の適用

	test2();
	

	myMarkSquare(PTT1_MARK_ID); // 動作確認用コード

		/***************************/
		/*                         */
		/*       draw関数をかけ    */
		/*                         */
		/*                         */
		/***************************/






	glDisable( GL_LIGHTING );	// 光源の無効化
	glDisable( GL_DEPTH_TEST );	// 陰面処理の無効化
}


// 光源の設定
void mySetLight( void ){
	GLfloat light_diffuse[]  = { 0.9, 0.9, 0.9, 1.0 }; // 拡散反射光
	GLfloat light_specular[] = { 1.0, 1.0, 1.0, 1.0 }; // 鏡面反射光
	GLfloat light_ambient[]  = { 0.3, 0.3, 0.3, 0.1 }; // 環境光
	GLfloat light_position[] = { 0.0, 100.0, 50.0, 1.0 }; // 位置と種類

	/* 光源の設定 */
	glLightfv( GL_LIGHT0, GL_DIFFUSE, light_diffuse );
	glLightfv( GL_LIGHT0, GL_SPECULAR, light_specular );
	glLightfv( GL_LIGHT0, GL_AMBIENT, light_ambient );
	glLightfv( GL_LIGHT0, GL_POSITION, light_position );

	glShadeModel( GL_SMOOTH ); // シェーディングの種類の設定
	glEnable( GL_LIGHT0 ); // 光源の有効化
}


void mySetMaterial( GLfloat R, GLfloat G, GLfloat B){
	GLfloat mat_diffuse[]  = { R, G, B, 1.0 };	// 拡散光の反射係数
	GLfloat mat_specular[] = { R, G, B, 1.0 };	// 鏡面光の反射係数
	GLfloat mat_ambient[]  = { 0.3, 0.3, 0.3, 1.0 };	// 環境光の反射係数
	GLfloat shininess[]    = { 50.0 };					// 鏡面光の指数

	/* 材質特性 */
	glMaterialfv( GL_FRONT, GL_DIFFUSE, mat_diffuse );		// 拡散反射の設定
	glMaterialfv( GL_FRONT, GL_SPECULAR, mat_specular );	// 鏡面反射の設定
	glMaterialfv( GL_FRONT, GL_AMBIENT, mat_ambient );		// 環境反射の設定
	glMaterialfv( GL_FRONT, GL_SHININESS, shininess );		// 鏡面反射の鋭さの設定
}


// 3Dオブジェクトを描画(引数によって変化)
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


// 引数によって色を変化させる
void mySelectColor(GLfloat R, GLfloat G, GLfloat B){
	mySetMaterial( R, G, B);
}


/* マーカを縁取る正方形を描画する関数 */
/* 線の色は設定していない             */
void myMarkSquare(int patt_id){
	glPushMatrix();
	myMatrix(object[patt_id].patt_trans);
		glLineWidth(3.0);             // 線の太さを設定
		glBegin( GL_LINE_LOOP );
			glVertex3f( -object[patt_id].patt_width/2.0, -object[patt_id].patt_width/2.0, 0.0 );
			glVertex3f(  object[patt_id].patt_width/2.0, -object[patt_id].patt_width/2.0, 0.0 );
			glVertex3f(  object[patt_id].patt_width/2.0,  object[patt_id].patt_width/2.0, 0.0 );
			glVertex3f( -object[patt_id].patt_width/2.0,  object[patt_id].patt_width/2.0, 0.0 );
		glEnd();
	glPopMatrix();
}


// 行列を s から d にコピーする
void myCopyTrans (double s[3][4], double d[3][4]){
	int i, k;

	for(i=0; i < 3; i++){
		for(k = 0; k < 4; k++){
			d[i][k] = s[i][k];
		}
	}
}


/* 任意のpattが出現したか */
int isAppearPatt(int patt_id){
	static int before[PTT_NUM]; // 1つ前のフレームでの各パットのvisible値 
	static int isFirst = 1; // この関数が初めて使用されるなら1
	int i;
	int v = 0;
	
	if (isFirst){ // before の初期化
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


/* 任意のpattが消失したか */
int isDisappearPatt(int patt_id){
	static int before[PTT_NUM]; // 1つ前のフレームでの各パットのvisible値 
	static int isFirst = 1; // この関数が初めて使用されるなら1
	int i;
	int v = 0;
	
	if (isFirst){ // before の初期化
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


/* すべてのpattについて出現検査   */
/* 引数appvにそれぞれの結果が格納 */
void roundAppearPatt(int appv[PTT_NUM]){
	int i;
	
	for (i=0; i<PTT_NUM; i++){
		appv[i] = isAppearPatt(i);
	}
}


/* すべてのpattについて消失検査   */
/* 引数appvにそれぞれの結果が格納 */
void roundDisappearPatt(int disv[PTT_NUM]){
	int i;
	
	for (i=0; i<PTT_NUM; i++){
		disv[i] = isDisappearPatt(i);
	}
}

void Initialize(void){
	texture = pngBind ( texname, PNG_NOMIPMAP, PNG_ALPHA, &info, GL_CLAMP, GL_NEAREST, GL_NEAREST);
}