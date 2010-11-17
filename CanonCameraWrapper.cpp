#include "CanonCameraWrapper.h"


    //---------------------------------------------------------------------
    CanonCameraWrapper::CanonCameraWrapper(){
        theCamera               = NULL;
        theCameraList           = NULL;

        bDeleteAfterDownload    = false;
        needToOpen              = false;
        registered              = false;
        downloadEnabled         = true;

        lastImageName           = "";
        lastImagePath           = "";

        evfMode                 = 0;
        device                  = kEdsEvfOutputDevice_TFT;

        livePixels              = NULL;
        livePixelsWidth         = 0;
        livePixelsHeight        = 0;

        resetLiveViewFrameCount();

        state = CAMERA_UNKNOWN;
    }

    //---------------------------------------------------------------------
    CanonCameraWrapper::~CanonCameraWrapper(){
        destroy();
    }

    //---------------------------------------------------------------------
    //
    //  SDK AND SESSION MANAGEMENT
    //
    //---------------------------------------------------------------------

    //---------------------------------------------------------------------
    bool CanonCameraWrapper::setup(int cameraID){
        if( theCamera != NULL || theCameraList != NULL){
            destroy();
        }

        EdsError err = EDS_ERR_OK;
        EdsUInt32 cameraCount = 0 ;

        err = EdsInitializeSDK();

        if(err != EDS_ERR_OK){
           printf("Couldn't open sdk!\n");
            return false;
        }else{
            printf("Opening the sdk\n");
            sdkRef++;
        }
		
		ofSleepMillis(3000);

        // Initialize
        // Get the camera list
        err = EdsGetCameraList(&theCameraList);

        // Get the number of cameras.
        if( err == EDS_ERR_OK ){
            err = EdsGetChildCount( theCameraList, &cameraCount );
            if ( cameraCount == 0 ){
                err = EDS_ERR_DEVICE_NOT_FOUND;
                printf("No devices found!\n");
                return false;
            }
        }

        // Get the camera
        if ( err == EDS_ERR_OK ){
            if (cameraID >= cameraCount){
                printf("No camera of id %i exists - number of cameras is %i\n", cameraID, cameraCount);
                return false;
            }

            printf("We are opening camera %i!\n", cameraID);

            err = EdsGetChildAtIndex( theCameraList , cameraID , &theCamera );
			
			//Release camera list
			if(theCameraList != NULL){
				EdsRelease(theCameraList);
			}
			
            if(err == EDS_ERR_OK){
                printf("We are connected!\n");
                state = CAMERA_READY;
                //return true;
            }else{
                printf("We are not connected!\n");
                state = CAMERA_UNKNOWN;
                return false;
            }
			
            registerCallback();		
			return true;			
        }
    }

    //---------------------------------------------------------------------
    void CanonCameraWrapper::destroy(){
		if( getLiveViewActive() ){
			endLiveView();
		}
		
        if( theCamera != NULL){
            closeSession();
        }

        easyRelease(theCamera);
        easyRelease(theCameraList);

        if( sdkRef > 0 ){
            sdkRef--;
            if( sdkRef == 0 ){
                EdsTerminateSDK();
                printf("Terminating the sdk\n");
            }
        }
    }

    //---------------------------------------------------------------------
    bool CanonCameraWrapper::openSession(){
        EdsError err = EDS_ERR_OK;

        err = EdsOpenSession( theCamera );
        if(err == EDS_ERR_OK){
            printf("We are opening session!\n");
            state = CAMERA_OPEN;
            return true;
        }
        else{
            printf("We failed at opening session!\n");
        }
        return false;
    }

    //---------------------------------------------------------------------
    bool CanonCameraWrapper::closeSession(){
        if( state == CAMERA_CLOSED)return false;
        EdsError err = EDS_ERR_OK;

        err = EdsCloseSession( theCamera );
        if(err == EDS_ERR_OK){
            printf("We are closing session!\n");
            state = CAMERA_CLOSED;
            return true;
        }
        else{
            printf("We failed at closing session!\n");
        }
        return false;
    }

    //---------------------------------------------------------------------
    //
    //  CONFIG
    //
    //---------------------------------------------------------------------

    //---------------------------------------------------------------------
    void CanonCameraWrapper::setDeleteFromCameraAfterDownload(bool deleteAfter){
        bDeleteAfterDownload = deleteAfter;
    }

    //---------------------------------------------------------------------
    void CanonCameraWrapper::setDownloadPath(string downloadPathStr){
        downloadPath = downloadPathStr;
        if( downloadPath != "" ){
            if(downloadPath[ downloadPath.length()-1 ] != '/' ){
                downloadPath  = downloadPath + "/";
            }
        }
    }

    //--------------------------------------------------------------------
    void CanonCameraWrapper::enableDownloadOnTrigger(){
        downloadEnabled = true;
    }

    //--------------------------------------------------------------------
    void CanonCameraWrapper::disableDownloadOnTrigger(){
        downloadEnabled = false;
    }

    //---------------------------------------------------------------------
    //
    //  ACTIONS
    //
    //---------------------------------------------------------------------

    //---------------------------------------------------------------------
    bool CanonCameraWrapper::takePicture(){
		return sendCommand(kEdsCameraCommand_TakePicture, 0);
    }
	
	bool CanonCameraWrapper::sendCommand( EdsCameraCommand inCommand,  EdsInt32 inParam){
      
		EdsError err = EDS_ERR_OK;
		
		if( preCommand() ){
			err = EdsSendCommand(theCamera, inCommand, inParam);
		
			postCommand();
			
			if(err == EDS_ERR_OK){
				return true;
			}
						
			if(err == EDS_ERR_DEVICE_BUSY){
				printf("CanonCameraWrapper::sendCommand - EDS_ERR_DEVICE_BUSY\n");
				return false;
			}
			
		}
		
		return false;
	}

    //---------------------------------------------------------------------
    //
    //  LIVE VIEW
    //
    //---------------------------------------------------------------------

    /*

        bool beginLiveView();                   //starts live view
    bool endLiveView();                     //ends live view

    bool grabPixelsFromLiveView();           //capture the live view to rgb pixel array
    bool saveImageFromLiveView(string saveName);

    bool getLiveViewActive();               //true if live view is enabled
    int getLiveViewFrameNo();               //returns the number of live view frames passed
    void resetLiveViewFrameCount();         //resets to 0

    bool isLiveViewPixels();                //true if there is captured pixels
    int getLiveViewPixelWidth();            //width of live view pixel data
    int getLiveViewPixelHeight();           //height of live view pixel data
    unsigned char * getLiveViewPixels();    //returns captured pixels

    */

    //---------------------------------------------------------------------
    bool CanonCameraWrapper::beginLiveView(){
        printf("Begining live view\n");

        preCommand();
	

        EdsError err = EDS_ERR_OK;

		if(evfMode == 0){
			evfMode = 1;
			// Set to the camera.
			err = EdsSetPropertyData(theCamera, kEdsPropID_Evf_Mode, 0, sizeof(evfMode), &evfMode);			
		}

		if( err == EDS_ERR_OK){
			// Set the PC as the current output device.
			device = kEdsEvfOutputDevice_PC;

			// Set to the camera.
			err = EdsSetPropertyData(theCamera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
		}

		//Notification of error
		if(err != EDS_ERR_OK){
			// It doesn't retry it at device busy
			if(err == EDS_ERR_DEVICE_BUSY){
				printf("BeginLiveView - device is busy\n");
				//CameraEvent e("DeviceBusy");
				//_model->notifyObservers(&e);
			}else{
				printf("BeginLiveView - device is busy\n");
			}
			return false;
		}
    }

    //---------------------------------------------------------------------
    bool CanonCameraWrapper::endLiveView(){
        printf("Ending live view\n");

        EdsError err = EDS_ERR_OK;

		if( err == EDS_ERR_OK){
			// Set the PC as the current output device.
			device = kEdsEvfOutputDevice_TFT;

			// Set to the camera.
			err = EdsSetPropertyData(theCamera, kEdsPropID_Evf_OutputDevice, 0, sizeof(device), &device);
		}

		if(evfMode == 1){
			evfMode = 0;
			// Set to the camera.
			err = EdsSetPropertyData(theCamera, kEdsPropID_Evf_Mode, 0, sizeof(evfMode), &evfMode);
		}

		bool success = true;

		//Notification of error
		if(err != EDS_ERR_OK){
			// It doesn't retry it at device busy
			if(err == EDS_ERR_DEVICE_BUSY){
				printf("EndLiveView - device is busy\n");
				//CameraEvent e("DeviceBusy");
				//_model->notifyObservers(&e);
			}else{
				printf("EndLiveView - device is busy\n");
			}
			success = false;
		}

		postCommand();

		return success;
    }

    //---------------------------------------------------------------------
    bool CanonCameraWrapper::grabPixelsFromLiveView(int rotateByNTimes90){
        EdsError err                = EDS_ERR_OK;
        EdsEvfImageRef evfImage     = NULL;
		EdsStreamRef stream         = NULL;
		EdsUInt32 bufferSize        = 2 * 1024 * 1024;

        bool success = false;

		if( evfMode == 0 ){
            printf("grabPixelsFromLiveView - live view needs to be enabled first\n");
            return false;
		}

		// Create memory stream.
		err = EdsCreateMemoryStream(bufferSize, &stream);

		// Create EvfImageRef.
		if (err == EDS_ERR_OK){
			err = EdsCreateEvfImageRef(stream, &evfImage);
		}

		// Download live view image data.
		if (err == EDS_ERR_OK){
			err = EdsDownloadEvfImage(theCamera, evfImage);
		}

        if (err == EDS_ERR_OK){
           // printf("Got live view frame %i \n", liveViewCurrentFrame);
            liveViewCurrentFrame++;

            EdsUInt32 length;
            EdsGetLength(stream, &length);

            if( length > 0 ){

                unsigned char * ImageData;
                EdsUInt32 DataSize = length;

                EdsGetPointer(stream,(EdsVoid**)&ImageData);
                EdsGetLength(stream, &DataSize);

                memoryImage convertor;
                if( convertor.loadFromMemory((int)DataSize, ImageData, rotateByNTimes90) ){

                    int imageWidth  = convertor.width;
                    int imageHeight = convertor.height;

                    if( imageWidth > 0 && imageHeight > 0 ){

                        if( livePixels == NULL || ( livePixelsWidth != imageWidth || livePixelsHeight != imageHeight ) ){
                            if( livePixels != NULL )delete[] livePixels;
                            livePixels          = new unsigned char[imageWidth * imageHeight * 3];
                            livePixelsWidth     = imageWidth;
                            livePixelsHeight    = imageHeight;
                        }

                        unsigned char * pix = convertor.getPixels();

                        int numToCopy = imageWidth * imageHeight * 3;
                        for(int i = 0; i < numToCopy; i++){
                            livePixels[i] = pix[i];
                        }

                        //printf("Live view frame converted to pixels\n");
                        success = true;

                    }else{
                        printf("unable to convert the memory stream! width and height 0 \n");
                    }
                }else{
                    printf("Unable to load from memory\n");
                }
            }
        }

        easyRelease(stream);
        easyRelease(evfImage);

		//Notification of error
		if(err != EDS_ERR_OK){
			if(err == EDS_ERR_OBJECT_NOTREADY){
			    printf("saveImageFromLiveView - not ready\n");
			}else if(err == EDS_ERR_DEVICE_BUSY){
                printf("saveImageFromLiveView - device is busy\n");
			}
            else{
                //printf("saveImageFromLiveView - some other error\n");
            }
            return false;
		}

		return success;
    }

    //---------------------------------------------------------------------
    bool CanonCameraWrapper::saveImageFromLiveView(string saveName){
        EdsError err                = EDS_ERR_OK;
        EdsEvfImageRef evfImage     = NULL;
		EdsStreamRef stream         = NULL;

		if( evfMode == 0 ){
            printf("Live view needs to be enabled first\n");
            return false;
		}

        //save the file stream to disk
		err = EdsCreateFileStream( ( ofToDataPath(saveName) ).c_str(), kEdsFileCreateDisposition_CreateAlways, kEdsAccess_ReadWrite, &stream);

		// Create EvfImageRef.
		if (err == EDS_ERR_OK){
			err = EdsCreateEvfImageRef(stream, &evfImage);
		}

		// Download live view image data.
		if (err == EDS_ERR_OK){
			err = EdsDownloadEvfImage(theCamera, evfImage);
		}

        if (err == EDS_ERR_OK){
            printf("Got live view frame %i \n", liveViewCurrentFrame);
            liveViewCurrentFrame++;
        }

        easyRelease(stream);
        easyRelease(evfImage);

		//Notification of error
		if(err != EDS_ERR_OK){
			if(err == EDS_ERR_OBJECT_NOTREADY){
			    printf("saveImageFromLiveView - not ready\n");
			}else if(err == EDS_ERR_DEVICE_BUSY){
                printf("saveImageFromLiveView - device is busy\n");
			}
            else{
                printf("saveImageFromLiveView - some other error\n");
            }
            return false;
		}

		return true;
    }

    //---------------------------------------------------------------------
    bool CanonCameraWrapper::getLiveViewActive(){
        return evfMode;
    }

    //---------------------------------------------------------------------
    int CanonCameraWrapper::getLiveViewFrameNo(){
        return liveViewCurrentFrame;
    }

    //---------------------------------------------------------------------
    void CanonCameraWrapper::resetLiveViewFrameCount(){
        liveViewCurrentFrame = 0;
    }

    //---------------------------------------------------------------------
    bool CanonCameraWrapper::isLiveViewPixels(){
        return (livePixels != NULL);
    }

    //---------------------------------------------------------------------
    int CanonCameraWrapper::getLiveViewPixelWidth(){
        return livePixelsWidth;
    }

    //--------------------------------------------------------------------
    int CanonCameraWrapper::getLiveViewPixelHeight(){
        return livePixelsHeight;
    }

    //---------------------------------------------------------------------
    unsigned char * CanonCameraWrapper::getLiveViewPixels(){
        return livePixels;
    }


    //---------------------------------------------------------------------
    //
    //  MISC EXTRA STUFF
    //
    //---------------------------------------------------------------------

    //---------------------------------------------------------------------
    string CanonCameraWrapper::getLastImageName(){
        return lastImageName;
    }

    //---------------------------------------------------------------------
    string CanonCameraWrapper::getLastImagePath(){
        return lastImagePath;
    }

    //This doesn't work perfectly - for some reason it can be one image behind
    //something about how often the camera updates the SDK.
    //Having the on picture event registered seems to help.
    //But downloading via event is much more reliable at the moment.

    //---------------------------------------------------------------------
    bool CanonCameraWrapper::downloadLastImage(){
        preCommand();

        EdsVolumeRef 		theVolumeRef	    = NULL ;
        EdsDirectoryItemRef	dirItemRef_DCIM	    = NULL;
        EdsDirectoryItemRef	dirItemRef_Sub	    = NULL;
        EdsDirectoryItemRef	dirItemRef_Image    = NULL;

        EdsDirectoryItemInfo dirItemInfo_Image;

        EdsError err    = EDS_ERR_OK;
        EdsUInt32 Count = 0;
        bool success    = false;

        //get the number of memory devices
        err = EdsGetChildCount( theCamera, &Count );
        if( Count == 0 ){
            printf("Memory device not found\n");
            err = EDS_ERR_DEVICE_NOT_FOUND;
            return false;
        }

        // Download Card No.0 contents
        err = EdsGetChildAtIndex( theCamera, 0, &theVolumeRef );
//        if ( err == EDS_ERR_OK ){
//            printf("getting volume info\n");
//            //err = EdsGetVolumeInfo( theVolumeRef, &volumeInfo ) ;
//        }

        //Now lets find out how many Folders the volume has
        if ( err == EDS_ERR_OK ){
            err = EdsGetChildCount( theVolumeRef, &Count );

            if ( err == EDS_ERR_OK ){

                //Lets find the folder called DCIM
                bool bFoundDCIM = false;
                for(int i = 0; i < Count; i++){
                    err = EdsGetChildAtIndex( theVolumeRef, i, &dirItemRef_DCIM ) ;
                    if ( err == EDS_ERR_OK ){
                        EdsDirectoryItemInfo dirItemInfo;
                        err = EdsGetDirectoryItemInfo( dirItemRef_DCIM, &dirItemInfo );
                        if( err == EDS_ERR_OK){
                            string folderName = dirItemInfo.szFileName;
                            if( folderName == "DCIM" ){
                                bFoundDCIM = true;
                                printf("Found the DCIM folder at index %i\n", i);
                                break;
                            }
                        }
                    }
                    //we want to release the directories that don't match
                    easyRelease(dirItemRef_DCIM);
                }

                //This is a bit silly.
                //Essentially we traverse into the DCIM folder, then we go into the last folder in there, then we
                //get the last image in last folder.
                if( bFoundDCIM && dirItemRef_DCIM != NULL){
                    //now we are going to look for the last folder in DCIM
                    Count = 0;
                    err = EdsGetChildCount(dirItemRef_DCIM, &Count);

                    bool foundLastFolder = false;
                    if( Count > 0 ){
                        int lastIndex = Count-1;

                        EdsDirectoryItemInfo dirItemInfo_Sub;

                        err = EdsGetChildAtIndex( dirItemRef_DCIM, lastIndex, &dirItemRef_Sub ) ;
                        err = EdsGetDirectoryItemInfo( dirItemRef_Sub, &dirItemInfo_Sub);

                        printf("Last Folder is %s \n", dirItemInfo_Sub.szFileName);

                        EdsUInt32 jpgCount = 0;
                        err = EdsGetChildCount(dirItemRef_Sub, &jpgCount );

                        if( jpgCount > 0 ){
                            int latestJpg = jpgCount-1;

                            err = EdsGetChildAtIndex(dirItemRef_Sub, latestJpg, &dirItemRef_Image ) ;
                            err = EdsGetDirectoryItemInfo(dirItemRef_Image, &dirItemInfo_Image);

                            printf("Latest image is %s \n", dirItemInfo_Image.szFileName);
                            success = true;
                        }else{
                            printf("Error - No jpegs inside %s\n", dirItemInfo_Image.szFileName);
                        }
                    }else{
                        printf("Error - No subfolders inside DCIM!\n");
                    }
                }
            }
        }
        if( success ){
            success = downloadImage(dirItemRef_Image);
        }

        easyRelease(theVolumeRef);
        easyRelease(dirItemRef_DCIM);
        easyRelease(dirItemRef_Sub);
        easyRelease(dirItemRef_Image);

        postCommand();

        return success;
    }

    //Hmm - might be needed for threading
    //---------------------------------------------------------------------
    bool CanonCameraWrapper::isTransfering(){
        return false;
    }




    //PROTECTED FUNCTIONS

    //---------------------------------------------------------------------
    bool CanonCameraWrapper::downloadImage(EdsDirectoryItemRef directoryItem){
        if( !downloadEnabled ) return false;

        EdsError err = EDS_ERR_OK;
        EdsStreamRef stream = NULL;
        EdsDirectoryItemInfo dirItemInfo;

        bool success = false;
        string imageName;
        string imagePath;

        int timeStart = ofGetElapsedTimeMillis();

        err = EdsGetDirectoryItemInfo(directoryItem, &dirItemInfo);
        if(err == EDS_ERR_OK){

            imageName = dirItemInfo.szFileName;
            imagePath = downloadPath + imageName;

            printf("Downloading image %s to %s\n", imageName.c_str(), imagePath.c_str());
            err = EdsCreateFileStream( ofToDataPath( imagePath ).c_str(), kEdsFileCreateDisposition_CreateAlways, kEdsAccess_ReadWrite, &stream);
        }

        if(err == EDS_ERR_OK){
            err = EdsDownload( directoryItem, dirItemInfo.size, stream);
        }

        if(err == EDS_ERR_OK){

            lastImageName = imageName;
            lastImagePath = imagePath;

            printf("Image downloaded in %ims\n", ofGetElapsedTimeMillis()-timeStart);

            err = EdsDownloadComplete(directoryItem);
            if( bDeleteAfterDownload ){
                printf("Image deleted\n");
                EdsDeleteDirectoryItem(directoryItem);
            }
            success = true;
        }

        easyRelease(stream);
        return success;
    }


    //------------------------------------------------------------------------
    EdsError EDSCALLBACK CanonCameraWrapper::handleObjectEvent(EdsObjectEvent event, EdsBaseRef object, EdsVoid *context) {
        printf("Callback! %i\n", (int)event);

        if(event == kEdsObjectEvent_DirItemContentChanged) {
            printf("kEdsObjectEvent_DirItemContentChanged!\n");
        }
        if(event == kEdsObjectEvent_DirItemRequestTransferDT) {
            printf("kEdsObjectEvent_DirItemRequestTransferDT!\n");
        }
        if(event == kEdsObjectEvent_DirItemCreated) {
            printf("kEdsObjectEvent_DirItemCreated!\n");
            CanonCameraWrapper * ptr = (CanonCameraWrapper *)context;
            ptr->downloadImage(object);
        }
        if(event == kEdsObjectEvent_FolderUpdateItems) {
            printf("kEdsObjectEvent_FolderUpdateItems!\n");
        }
        if(event == kEdsObjectEvent_VolumeUpdateItems) {
            printf("kEdsObjectEvent_VolumeUpdateItems!\n");
        }
        if(event == kEdsObjectEvent_DirItemRequestTransfer) {
            printf("kEdsObjectEvent_DirItemRequestTransfer!\n");
        }
    }

    //------------------------------------------------------------------------
	EdsError EDSCALLBACK CanonCameraWrapper::handlePropertyEvent(EdsPropertyEvent inEvent,  EdsPropertyID inPropertyID, EdsUInt32 inParam, EdsVoid * inContext){
        printf("Callback! %i\n", (int)inEvent);
		
		string prop = "property not listed!";
		
		switch(inPropertyID){
			
			case  kEdsPropID_Unknown                : prop = "kEdsPropID_Unknown";
			break;
			case  kEdsPropID_ProductName            : prop = "kEdsPropID_ProductName";
			break;
			case  kEdsPropID_BodyID                 : prop = "kEdsPropID_BodyID";
			break;
			case  kEdsPropID_OwnerName              : prop = "kEdsPropID_OwnerName";
			break;
			case  kEdsPropID_MakerName              : prop = "kEdsPropID_MakerName";
			break;
			case  kEdsPropID_DateTime               : prop = "kEdsPropID_DateTime";
			break;
			case  kEdsPropID_FirmwareVersion        : prop = "kEdsPropID_FirmwareVersion";
			break;
			case  kEdsPropID_BatteryLevel           : prop = "kEdsPropID_BatteryLevel";
			break;
			case  kEdsPropID_CFn                    : prop = "kEdsPropID_CFn";
			break;
			case  kEdsPropID_SaveTo                 : prop = "kEdsPropID_SaveTo";
			break;
			case  kEdsPropID_CurrentStorage         : prop = "kEdsPropID_CurrentStorage";
			break;
			case  kEdsPropID_CurrentFolder          : prop = "kEdsPropID_CurrentFolder";
			break;
			case  kEdsPropID_MyMenu					: prop = "kEdsPropID_MyMenu";
			break;
			case  kEdsPropID_BatteryQuality         : prop = "kEdsPropID_BatteryQuality";
			break;	
			case  kEdsPropID_HDDirectoryStructure   : prop = "kEdsPropID_HDDirectoryStructure";
					
		}

        if(inEvent == kEdsPropertyEvent_PropertyChanged) {
            printf("kEdsPropertyEvent_PropertyChanged - %s!\n", prop.c_str());
        }
        if(inEvent == kEdsPropertyEvent_PropertyDescChanged) {
            printf("kEdsPropertyEvent_PropertyDescChanged - %s\n", prop.c_str());
        }
    }
	
	//----------------------------------------------------------------------------
	EdsError EDSCALLBACK CanonCameraWrapper::handleStateEvent(EdsStateEvent inEvent, EdsUInt32 inEventData, EdsVoid * inContext){
        if(inEvent == kEdsStateEvent_Shutdown) {
            printf("kEdsStateEvent_Shutdown\n");
        }
        if(inEvent == kEdsStateEvent_JobStatusChanged) {
            printf("kEdsStateEvent_JobStatusChanged");
		}
        if(inEvent == kEdsStateEvent_WillSoonShutDown) {
            printf("kEdsStateEvent_WillSoonShutDown");
		}	
        if(inEvent == kEdsStateEvent_ShutDownTimerUpdate) {
            printf("kEdsStateEvent_ShutDownTimerUpdate");
		}	
        if(inEvent == kEdsStateEvent_CaptureError) {
            printf("kEdsStateEvent_CaptureError");
		}	
        if(inEvent == kEdsStateEvent_InternalError) {
            printf("kEdsStateEvent_InternalError");
		}
        if(inEvent == kEdsStateEvent_AfResult) {
            printf("kEdsStateEvent_AfResult");
		}
        if(inEvent == kEdsStateEvent_BulbExposureTime) {
            printf("kEdsStateEvent_BulbExposureTime");
		}												
	}
	

    //----------------------------------------------------------------------------
    void CanonCameraWrapper::registerCallback(){
        if( registered == false){
				
			int err = EDS_ERR_OK;
			
			// Set event handler 
			if(err == EDS_ERR_OK) {
				err = EdsSetObjectEventHandler(theCamera,	kEdsObjectEvent_All, handleObjectEvent, this);
			}else{
				printf("Unable to set callback EdsSetObjectEventHandler\n");
			}
			
			// Set event handler 
			if(err == EDS_ERR_OK) {
				err = EdsSetPropertyEventHandler(theCamera,	kEdsPropertyEvent_All, handlePropertyEvent, this);
			}else{
				printf("Unable to set callback EdsSetPropertyEventHandler\n");
			}
			
			// Set event handler 
			if(err == EDS_ERR_OK) {
				err = EdsSetCameraStateEventHandler(theCamera,	kEdsStateEvent_All, handleStateEvent, this);
			}else{
				printf("Unable to set callback EdsSetPropertyEventHandler\n");
			}
		
		}		
		
        registered = true;
    }


    //PRE AND POST COMMAND should be used at the begining and the end of camera interaction functions
    //EG: taking a picture or starting / ending live view.
    //
    //They check if their is an open session to the camera and if there isn't they create a session
    //postCommand closes any sessions created by preCommand
    //
    // eg:
    // preCommand
    // takePicture
    // postCommand
    //---------------------------------------------------------------------
    bool CanonCameraWrapper::preCommand(){

        printf("pre command \n");

        if( state > CAMERA_UNKNOWN ){
            needToOpen = false;
            bool readyToGo  = false;
			
            if( state != CAMERA_OPEN ){
                needToOpen = true;
            }else{
                readyToGo = true;
                return true;
            }

            if( needToOpen ){
                readyToGo = openSession();
            }

            return readyToGo;

        }else{
            return false;
        }

    }

    //---------------------------------------------------------------------
    void CanonCameraWrapper::postCommand(){
        printf("post command \n");

        if(state == CAMERA_OPEN && needToOpen){
			printf("postCommand - closing session\n");
            closeSession();
        }
    }
