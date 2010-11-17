#pragma once

//Written by Theo Watson - theo@openframeworks.cc

//NOTE
//We are missing code for legacy devices
//We are missing some mac specific snippets
//Both should be easy to integrate

//You also need the Canon SDK which you can request from them or possibily find by other means floating around in the ether. 

#include "EDSDK.h"
#include "EDSDKErrors.h"
#include "EDSDKTypes.h"

#define HAVE_OF

#ifdef HAVE_OF
    #include "ofImage.h"
#endif

#ifndef BYTE
	#define BYTE unsigned char
#endif
	
typedef enum{
    CAMERA_UNKNOWN,
    CAMERA_READY,
    CAMERA_OPEN,
    CAMERA_CLOSED,
}cameraState;

static int sdkRef = 0;

static void easyRelease(EdsBaseRef &ref){
    if(ref != NULL){
        EdsRelease(ref);
        ref = NULL;
    }
}

class memoryImage : public ofImage{

    public:

    bool loadFromMemory(int bytesToRead, unsigned char * jpegBytes, int rotateMode = 0){
        FIMEMORY *hmem = NULL;

        hmem = FreeImage_OpenMemory((BYTE *)jpegBytes, bytesToRead);
        if (hmem == NULL){
            printf("couldn't create memory handle! \n");
            return false;
        }

        //get the file type!
        FREE_IMAGE_FORMAT fif = FreeImage_GetFileTypeFromMemory(hmem);
        if( fif == -1 ){
            printf("unable to guess format", fif);
            return false;
            FreeImage_CloseMemory(hmem);
        }

        //make the image!!
        FIBITMAP * tmpBmp = FreeImage_LoadFromMemory(fif, hmem, 0);

        if( rotateMode > 0 && rotateMode < 4){
            FIBITMAP * oldBmp = tmpBmp;

            if( rotateMode == 1)tmpBmp = FreeImage_RotateClassic(tmpBmp, 90);
            if( rotateMode == 2)tmpBmp = FreeImage_RotateClassic(tmpBmp, 180);
            if( rotateMode == 3)tmpBmp = FreeImage_RotateClassic(tmpBmp, 270);

            FreeImage_Unload(oldBmp);
        }

        //FreeImage_FlipVertical(tmpBmp);

        putBmpIntoPixels(tmpBmp, myPixels);
        width 		= FreeImage_GetWidth(tmpBmp);
        height 		= FreeImage_GetHeight(tmpBmp);
        bpp 		= FreeImage_GetBPP(tmpBmp);

        swapRgb(myPixels);

        FreeImage_Unload(tmpBmp);
        FreeImage_CloseMemory(hmem);

        return true;
    }


    //shouldn't have to redefine this but a gcc bug means we do
    inline void	swapRgb(ofPixels &pix){
        if (pix.bitsPerPixel != 8){
            int sizePixels		= pix.width*pix.height;
            int cnt				= 0;
            unsigned char temp;
            int byteCount		= pix.bitsPerPixel/8;

            while (cnt < sizePixels){
                temp					        = pix.pixels[cnt*byteCount];
                pix.pixels[cnt*byteCount]		= pix.pixels[cnt*byteCount+2];
                pix.pixels[cnt*byteCount+2]		= temp;
                cnt++;
            }
        }
    }

};

//NOTE
//We are missing code for legacy devices
//We are missing some mac specific snippets
//Both should be easy to integrate

class CanonCameraWrapper{
    public:

 //---------------------------------------------------------------------
    CanonCameraWrapper();
    ~CanonCameraWrapper();

    //---------------------------------------------------------------------
    //  SDK AND SESSION MANAGEMENT
    //---------------------------------------------------------------------
    bool setup(int cameraID);   //You must call this to init the canon sdk
    void destroy();             //To clean up - also called by destructor

    bool openSession();         //Begins communication with camera
    bool closeSession();        //Ends communication with camera.
                                //Note on sessions: Commands like takePicture
                                //will open a session if none exists. This
                                //is slower though so consider calling it
                                //once at the begining of your app.

    //---------------------------------------------------------------------
    //  CONFIG
    //---------------------------------------------------------------------

    void setDeleteFromCameraAfterDownload(bool deleteAfter);
    void setDownloadPath(string downloadPathStr);
    void enableDownloadOnTrigger();     //Trigger meaning takePicture
    void disableDownloadOnTrigger();    //Trigger meaning takePicture

    //---------------------------------------------------------------------
    //  ACTIONS
    //---------------------------------------------------------------------
    bool takePicture();   //Takes a picture. If enabled it will also download
                          //the image to the folder set by the download path.
						  
	bool sendCommand( EdsCameraCommand inCommand,  EdsInt32 inParam = 0); 


    //---------------------------------------------------------------------
    //  LIVE VIEW
    //---------------------------------------------------------------------

    bool beginLiveView();                   //starts live view
    bool endLiveView();                     //ends live view

    bool grabPixelsFromLiveView(int rotateByNTimes90 = 0); //capture the live view to rgb pixel array
    bool saveImageFromLiveView(string saveName);

    bool getLiveViewActive();               //true if live view is enabled
    int getLiveViewFrameNo();               //returns the number of live view frames passed
    void resetLiveViewFrameCount();         //resets to 0

    bool isLiveViewPixels();                //true if there is captured pixels
    int getLiveViewPixelWidth();            //width of live view pixel data
    int getLiveViewPixelHeight();           //height of live view pixel data
    unsigned char * getLiveViewPixels();    //returns captured pixels

    //---------------------------------------------------------------------
    //  MISC EXTRA STUFF
    //---------------------------------------------------------------------

    string getLastImageName();  //The full path of the last downloaded image
    string getLastImagePath();  //The name of the last downloaded image

    //This doesn't work perfectly - for some reason it can be one image behind
    //something about how often the camera updates the SDK.
    //Having the on picture event registered seems to help.
    //But downloading via event is much more reliable at the moment.

    //WARNING - If you are not taking pictures and you have bDeleteAfterDownload set to true
    //you will be deleting the files that are on the camera.
    //Simplified: be careful about calling this when you haven't just taken a photo.
    bool downloadLastImage();

    //Hmm - might be needed for threading - currently doesn't work
    bool isTransfering();


    protected:
        //---------------------------------------------------------------------
        //  PROTECTED STUFF
        //---------------------------------------------------------------------

        bool downloadImage(EdsDirectoryItemRef directoryItem);
        static EdsError EDSCALLBACK handleObjectEvent(EdsObjectEvent inEvent, EdsBaseRef object, EdsVoid *inContext);
        static EdsError EDSCALLBACK handlePropertyEvent(EdsPropertyEvent inEvent,  EdsPropertyID inPropertyID, EdsUInt32 inParam, EdsVoid * inContext);
        static EdsError EDSCALLBACK handleStateEvent(EdsStateEvent inEvent, EdsUInt32 inEventData, EdsVoid * inContext);
			
		
        void registerCallback();
        bool preCommand();
        void postCommand();

        int livePixelsWidth;
        int livePixelsHeight;
        unsigned char * livePixels;

        EdsUInt32 evfMode;
        EdsUInt32 device;

        int liveViewCurrentFrame;

        string lastImageName;
        string lastImagePath;
        string downloadPath;
        bool downloadEnabled;
        bool bDeleteAfterDownload;
        bool registered;
        bool needToOpen;

        cameraState state;
        EdsCameraRef        theCamera ;
        EdsCameraListRef	theCameraList;

};

