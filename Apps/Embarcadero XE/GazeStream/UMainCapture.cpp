/*
Main form for GazeStream.

Written by Matthew Grivich and Andre Vankov, Swartz Center for Computational Neuroscience, UCSD, 2011
*/
//---------------------------------------------------------------------------
#include "LinearAlgebra.h"
#include "useallegro4.h"
#include <vcl.h>
#pragma hdrstop
#include "drutil.h"
#include "UMainCapture.h"
#include "stdio.h"
#include "Shlobj.h"
#include "float.h"
#include "TFrameThread.h"
#include "CaptureWorkerForm.h"
#include "Math.hpp"
#include "StreamThread.h"
#include "console_util.h"


//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TMainCaptureForm *MainCaptureForm;
//---------------------------------------------------------------------------
__fastcall TMainCaptureForm::TMainCaptureForm(TComponent* Owner)
	: TForm(Owner)
{
}
//---------------------------------------------------------------------------


#include "CircleFunction.h"
#include "Fitter.h"

bool storeFit = false;


int nFrames;


/*
class TFrameStamp: public TMaxArray
{
	public:
	TFrameStamp()
	{
		this->nMaxItems = 10;
	}
};

TFrameStamp FrameStamp;

class TStimEventcode: public TMaxArray
{
	public:
	TStimEventcode()
	{
		this->nMaxItems = 1;
	}
};

TStimEventcode FrameNumber,Zero;
*/

TFrameThread *frameThread;
HANDLE hMutex = 0;

//HANDLE handleWr1 = 0;
//HANDLE handleWrScene = 0;
//HANDLE handleWrVideo = 0;
//HANDLE handleRd = 0;
lsl_outlet outlet = 0;
TStreamThread *gazestreamThread = 0;

BITMAP * bmpCanvas = NULL;		// User declarations

int frameDivisor = 1;
int spatialDivisor = 1;
double maxEccentricity = 1.0;
double xParallaxCorrection = 0.0;
double yParallaxCorrection = 0.0;


#define CDEPTH 24
 void __fastcall TMainCaptureForm::CheckBox1Click(TObject *Sender)
{
	allegro_init();
	set_gdi_color_format();
	set_color_depth(CDEPTH);
	bmpCanvas = create_bitmap_ex(CDEPTH,tPanel->Width,tPanel->Height);

	clear_bitmap(bmpCanvas);

}


void AssignListToComboBox (TComboBox* ComboBox, String List, int Index)
{
   ComboBox->Items->Text = List;
   if (ComboBox->Items->Count > 0) {
	  ComboBox->ItemIndex = Index;
   }
}


void __fastcall TMainCaptureForm::FormCreate(TObject *Sender)
{


	//There has to be some option other than silent failure and dialog box, but I haven't found it yet.
	//	_control87(MCW_EM, MCW_EM);  //Turn off FPU generated floating point exceptions. Threads still fail however.
	_control87( 0x1372, 0x137F ); //turns on dialog exceptions, but not in TThread. Exceptions in TThread cause the thread to die.


	pBmpRec=new Graphics::TBitmap;
	pBmpRec-> LoadFromFile("Rec.bmp");
	pBmpRecGr=new Graphics::TBitmap;
	pBmpRecGr-> LoadFromFile("Rec-gr.bmp");
	pBmpPau=new Graphics::TBitmap;
	pBmpPau->LoadFromFile("Pau.bmp");
	pBmpPauGr=new Graphics::TBitmap;
	pBmpPauGr->LoadFromFile("Pau-gr.bmp");

	//set default output folder to c:\Users\currentUser\Desktop\capture
	char pathC[MAX_PATH];
	SHGetSpecialFolderPath(NULL, pathC, CSIDL_DESKTOP, 1);
	edOutput->Text = UnicodeString(pathC) + "\\capture";

	frameThread = NULL;
	hMutex = CreateMutex(0,false,0);
	BitBtnStop->Enabled = false;
	nOutlinesEditChange(this);
	crRadiusMultiplierEditChange(this);
	crRadiusMaxEditChange(this);
	FrameDivisorEditChange(this);
	SpatialDivisorEditChange(this);
	maxEccentricityEditChange(this);
	xParallaxCorrectionEditChange(this);
	yParallaxCorrectionEditChange(this);
	gu = new TGazeUtil();


	allegro_init();
	set_gdi_color_format();
	set_color_depth(CDEPTH);

	nFrames=0;

}


int acqWidth=0, acqHeight=0;

void __fastcall TMainCaptureForm::cbVideoInputDeviceChange(TObject *Sender)
{
	if(cbVideoInputDevice->ItemIndex != -1) {
		CaptureWorkerForm->VideoGrabber->VideoDevice = cbVideoInputDevice->ItemIndex;
		AssignListToComboBox (cbVideoInputFormat, CaptureWorkerForm->VideoGrabber->VideoSizes, CaptureWorkerForm->VideoGrabber->VideoSize);
		cbVideoInputFormatChange(this);
	}
}

//---------------------------------------------------------------------------

void __fastcall TMainCaptureForm::cbVideoInputFormatChange(TObject *Sender)
{
	if (cbVideoInputFormat->ItemIndex != -1) {
		CaptureWorkerForm->VideoGrabber->VideoSize = cbVideoInputFormat->ItemIndex;
		acqWidth = CaptureWorkerForm->VideoGrabber->Display_Width;
		acqHeight = CaptureWorkerForm->VideoGrabber->Display_Height;


		tPanel->Width = acqWidth/spatialDivisor;

		crRoiRight->Max = acqWidth;
		if(crRoiRight->Position > acqWidth) crRoiRight->Position = acqWidth;

		tbRight->Max = acqWidth;
		if(tbRight->Position > acqWidth) tbRight->Position = acqWidth;

		crRoiLeft->Max = acqWidth;
		if(crRoiLeft->Position > acqWidth) crRoiLeft->Position = acqWidth;

		tbLeft->Max = acqWidth;
		if(tbLeft->Position > acqWidth) tbLeft->Position = acqWidth;

		tPanel->Height = acqHeight/spatialDivisor;

		crRoiBottom->Max = acqHeight;
		if(crRoiBottom->Position > acqHeight) crRoiBottom->Position = acqHeight;

		tbLower->Max = acqHeight;
		if(tbLower->Position > acqHeight) tbLower->Position = acqHeight;

		crRoiTop->Max = acqHeight;
		if(crRoiTop->Position > acqHeight) crRoiTop->Position = acqHeight;

		tbUpper->Max = acqHeight;
		if(tbUpper->Position > acqHeight) tbUpper->Position = acqHeight;

		if(bmpCanvas) {
			destroy_bitmap(bmpCanvas);
			bmpCanvas = NULL;
		}
		bmpCanvas = create_bitmap_ex(CDEPTH,acqWidth/spatialDivisor,acqHeight/spatialDivisor);
		clear_bitmap(bmpCanvas);
	}

}
//---------------------------------------------------------------------------

void __fastcall TMainCaptureForm::btStopClick(TObject *Sender)
{

	if(cbRecord->Checked == true) {
		CaptureWorkerForm->VideoGrabber->StopRecording();
	} else {
		CaptureWorkerForm->VideoGrabber->StopPreview();
	}

	BitBtnStop->Enabled=false;
	BitBtnPlay->Enabled=true;

	cbRecord->Enabled =true;

	frameThread->Terminate();
	delete frameThread; //will not delete till terminated, by VCL design.
	frameThread = NULL;

	nFrames = 0;

	cbVideoInputDevice->Enabled = true;
	cbVideoInputFormat->Enabled = true;
	SpatialDivisorEdit->Enabled = true;

}

//---------------------------------------------------------------------------

void __fastcall TMainCaptureForm::Start()
{

		CaptureWorkerForm->VideoGrabber->AnalogVideoStandard = CaptureWorkerForm->VideoGrabber->AnalogVideoStandardIndex ("NTSC M");
		cbRecord->Enabled =false;
		CaptureWorkerForm->VideoGrabber->RecordingInNativeFormat = false;
		if(cbRecord->Checked == true) {
			CaptureWorkerForm->SetQueue(bmpQueue, hMutex);

			//save to specified filename
			int n=0;
			UnicodeString outputFileName;
			while(true) {
				if(cbCompress->Checked)
					outputFileName =  edOutput->Text+ IntToStr(n)+".asf";
				else
					outputFileName =  edOutput->Text+ IntToStr(n)+".avi";

				if(!FileExists(outputFileName)) break;
				++n;
			}
			MkDir(outputFileName);
			if(DirectoryExists(outputFileName)) RmDir(outputFileName);
			else {
				Application->MessageBoxA(L"Invalid path for writing movie.", L"Error", MB_OK);
				return;
			}

			CaptureWorkerForm->VideoGrabber->RecordingFileName= outputFileName;

			//record audio?
			CaptureWorkerForm->VideoGrabber->AudioRecording = cbRecordAudio->Checked;

			//turn off some unused features
			CaptureWorkerForm->VideoGrabber->FrameGrabber = TFrameGrabber (0);

			//save as compressed asf
			CaptureWorkerForm->VideoGrabber->VideoCompressor = cbVideoCodecs->ItemIndex;

			if(cbCompress->Checked) {
				CaptureWorkerForm->VideoGrabber->CompressionMode = cm_CompressOnTheFly;
				CaptureWorkerForm->VideoGrabber->RecordingMethod = rm_ASF;
			} else {

				CaptureWorkerForm->VideoGrabber->CompressionMode = cm_NoCompression;
				CaptureWorkerForm->VideoGrabber->RecordingMethod = rm_AVI;
			}


			CaptureWorkerForm->VideoGrabber->HoldRecording = false;
		//	CaptureWorkerForm->VideoGrabber->VideoDevice = CaptureWorkerForm->VideoGrabber->VideoDeviceIndex("Dazzle DVC100 Video Device");
			CaptureWorkerForm->VideoGrabber->StartRecording();


		} else {
			CaptureWorkerForm->SetQueue(bmpQueue, hMutex);
			CaptureWorkerForm->VideoGrabber->StartPreview();
		}

}



void __fastcall TMainCaptureForm::FormDestroy(TObject *Sender)
{

	if(outlet) {
		lsl_destroy_outlet(outlet);
		outlet = NULL;
	}
	if(gazestreamThread) {
		gazestreamThread->Terminate();
		delete gazestreamThread;
		gazestreamThread = NULL;
	}
	/*
	if(handleWr1)
		ds_Close(handleWr1);
	if (handleWrScene)
		ds_Close(handleWrScene);
	if (handleWrVideo)
		ds_Close(handleWrVideo);
	if(handleRd)
		ds_Close(handleRd);
    */

	if(gu)
		delete gu;



}



enum TKind {isEye,isSceneCalib,isScene, isVideo} ;
TKind isKind = -1;


void drawEllipse(BITMAP *bmp, double x0, double y0, double rA, double rB, double theta, int color) {
	int nPoints = 20;
	double tempX, tempY;
	int x = 0;
	int y = 0;
	tempX = rA;
	tempY = 0;
	double oldX = rA*cos(theta)+x0;
	double oldY = rA*sin(theta)+y0;
		 //	circlefill(bmpCanvas, oldX, oldY, 3, makecol(0,0,0));
	for(int i=0; i<nPoints; i++) {
		tempX = rA*cos(2*PI*i/nPoints);
		tempY = rB*sin(2*PI*i/nPoints);
		x = (int) (tempX*cos(theta)-tempY*sin(theta)+x0);
		y = (int) (tempX*sin(theta)+tempY*cos(theta)+y0);
		line(bmp, oldX, oldY, x, y, color);
		oldX = x;
		oldY = y;
	}
	line(bmp, oldX, oldY, rA*cos(theta)+x0, rA*sin(theta)+y0, color);

}


TOutline* TMainCaptureForm::findLargestOutline(BITMAP *aBmp, boolean above,
	int tbLeftPosition, int tbRightPosition, int tbLowerPosition, int tbUpperPosition, bool paint) {

		int subsampling = SubsamplingEdit->Text.ToIntDef(10);
		int subWidth = (tbRightPosition - tbLeftPosition) /subsampling;
		int subHeight = (tbLowerPosition - tbUpperPosition)/subsampling;
		if(subWidth <=0 || subHeight <=0) {
			return new TOutline(subWidth, subHeight);
		}
		unsigned char ** subsampled = new2D<unsigned char>(subWidth, subHeight, 0);
		signed char ** mask = new2D<signed char>(subWidth, subHeight, 0);
		int xOffset = subsampling/2+tbLeftPosition;
		int yOffset =  subsampling/2+tbUpperPosition;
		int totalPoints = 0;

		boolean withinThresh = false;

		//subsample and threshold the image.
		for(int y=0; y<subHeight; y++) {
			for(int x=0; x<subWidth; x++) {
				int blue = aBmp->line[y*subsampling+yOffset]
					[(x*subsampling+xOffset)*(CDEPTH/8)];
				int green = aBmp->line[y*subsampling+yOffset]
					[(x*subsampling+xOffset)*(CDEPTH/8)+1];
				int red = aBmp->line[y*subsampling+yOffset]
					[(x*subsampling+xOffset)*(CDEPTH/8)+2];
				//if scene calib, look explicitly for a positive blue spot.
				if(isKind==isSceneCalib) {
					if(255*blue/(blue+green+red+.01) >= crThreshold->Position && blue >= crThreshold->Position) {
						subsampled[x][y] = 1;
						if(paint) {
							bmpCanvas->line[(y*subsampling+yOffset)/spatialDivisor]
								[(x*subsampling+xOffset)/spatialDivisor*(CDEPTH/8)] = 0;
							bmpCanvas->line[(y*subsampling+yOffset)/spatialDivisor]
								[(x*subsampling+xOffset)/spatialDivisor*(CDEPTH/8)+1] = 0;
							bmpCanvas->line[(y*subsampling+yOffset)/spatialDivisor]
								[(x*subsampling+xOffset)/spatialDivisor*(CDEPTH/8)+2] = 255;
						}
						totalPoints++;
					} else {
						subsampled[x][y] = 0;
					}
				} else if(isKind==isEye) {
					//used to find the cornea reflection. Points near the cornea reflection
					//are not included in the pupil fit.
					if(above) {
						//above "blue" but image is black and white.
						if(blue >=crThreshold->Position) {
							subsampled[x][y] = 1;
							if(paint) {
								bmpCanvas->line[(y*subsampling+yOffset)/spatialDivisor]
									[(x*subsampling+xOffset)/spatialDivisor*(CDEPTH/8)] = 0;
								bmpCanvas->line[(y*subsampling+yOffset)/spatialDivisor]
									[(x*subsampling+xOffset)/spatialDivisor*(CDEPTH/8)+1] = 0;
								bmpCanvas->line[(y*subsampling+yOffset)/spatialDivisor]
									[(x*subsampling+xOffset)/spatialDivisor*(CDEPTH/8)+2] = 255;
							}
							totalPoints++;
						} else {
							subsampled[x][y] = 0;
						}
					//used to find the pupil
					} else {
						//below "blue" but image is black and white.
						if(blue <=tbThreshold->Position) {
							subsampled[x][y] = 1;
							if(paint) {
								bmpCanvas->line[(y*subsampling+yOffset)/spatialDivisor]
									[(x*subsampling+xOffset)/spatialDivisor*(CDEPTH/8)] = 0;
								bmpCanvas->line[(y*subsampling+yOffset)/spatialDivisor]
									[(x*subsampling+xOffset)/spatialDivisor*(CDEPTH/8)+1] = 255;
								bmpCanvas->line[(y*subsampling+yOffset)/spatialDivisor]
									[(x*subsampling+xOffset)/spatialDivisor*(CDEPTH/8)+2] = 0;
							}
							totalPoints++;


						} else {
							subsampled[x][y] = 0;
						}
					}
				}



			}
		}
		//find outlines
		int pointsInLargest = 0;
		int nOutlines = 0;
		TOutline *largestOutline = new TOutline(subWidth, subHeight);
		for(int y=0; y<subHeight; y++) {
			int inSpot = 0;
			for(int x=0; x<subWidth; x++) {
				//inSpot and mask together keep track of boundary transitions.
				//mask[x][y] = +1 when a spot is entered, -1 when it is exited.
				//Donut holes are removed/ignored, because interior boundaries are never tested.
				inSpot = mask[x][y] + inSpot;
				if(inSpot == 0) {
					if(subsampled[x][y] == 1) {
						TOutline *outline = new TOutline(subWidth, subHeight);
						nOutlines++;
						int nPointsInSpot = outline->findOutline((const unsigned char **) subsampled, mask,x, y, bmpCanvas->line);
				//		outline->drawSpot(bmpCanvas->line, mask); for testing purposes

						//if new spot is larger than old spot
						if (nPointsInSpot > pointsInLargest) {
							outline->estimateParams();

							//if eccentricity is somewhat circle like. This cuts long edges is the data.
							if(!_isnan(outline->eccentricity) && outline->eccentricity < maxEccentricity
									&& !_isnan(outline->r)) {
								pointsInLargest = nPointsInSpot;
								delete largestOutline;
								largestOutline = outline;
							} else {
								delete outline;
							}

						} else {
							delete outline;
						}
						inSpot++;
					}
				}

			}
		}

		//If autothreshold is checked, try to keep the number of spots constant.
		//This is fairly robust to contrast and gain changes.
		if(autoThresholdBox->Checked == true && !above) {
			if(nOutlines >= nOutlinesDesired && tbThreshold->Position > 10) tbThreshold->Position--;
			if(nOutlines < nOutlinesDesired) tbThreshold->Position++;
		}
		delete2D(mask, subWidth);
		delete2D(subsampled, subWidth);
		return largestOutline;
}


void TMainCaptureForm::fitCircle(BITMAP *aBmp, TOutline *largestOutline, double *x0, double *y0, double *radius, double crX0, double crY0, double crRadius, boolean above,
	int tbLeftPosition, int tbRightPosition, int tbLowerPosition, int tbUpperPosition, bool paint) {

		int subsampling = SubsamplingEdit->Text.ToIntDef(10);
		int xOffset = subsampling/2+tbLeftPosition;
		int yOffset =  subsampling/2+tbUpperPosition;

		double ** x = largestOutline->createDataForFit((crX0-xOffset)/subsampling, (crY0-yOffset)/subsampling, crRadius/subsampling);
		double * y = new1D<double>(largestOutline->nPointsMinusCR, 0.0);
		double * sig = new1D<double>(largestOutline->nPointsMinusCR, 1.0);
 //		largestOutline->drawOutline(bmpCanvas->line,x,largestOutline->getNumberOfPoints()); for testing purposes
		Fitter *fit = new Fitter();
		//reduce dropped frames due to perverse distibutions.  Typically less than 10 iterations are needed.
		fit->maxIterations = 50;
		CircleFunction *f = NULL;

		try {
			if(largestOutline->nPointsMinusCR > 5) {

				f = new CircleFunction(largestOutline->x0, largestOutline->y0, largestOutline->r);
				fit->fit(f,x,y,sig,2,largestOutline->nPointsMinusCR);

				double *pOut = fit->getParameters();
				*x0 = pOut[0]*subsampling+xOffset;
				*y0 = pOut[1]*subsampling+yOffset;
				*radius = pOut[2]*subsampling;

				if(above) {
					*radius *= crRadiusMultiplier;
					if(*radius > crRadiusMax) *radius = crRadiusMax;
				}
				if(paint) {
					if(above)
						circle(bmpCanvas, *x0/spatialDivisor, *y0/spatialDivisor, *radius/spatialDivisor, makecol(0,255,0));
					else
						circle(bmpCanvas,*x0/spatialDivisor, *y0/spatialDivisor, *radius/spatialDivisor, makecol(255,0,0));
				}
			}
		} catch (exception& ex) {
 //			printf("caught umaincapture\n");
 //			printf("exception: %s\n", ex.what());
			*x0 = *y0 = *radius = 0.0;
		}
		delete2D(x, 2);
		delete1D(y);
		delete1D(sig);
		delete fit;
		delete f;
		delete largestOutline;

	}

 void TMainCaptureForm::fitEllipse(BITMAP *aBmp, TOutline *largestOutline, double *x0, double *y0, double *radiusA, double *radiusB, double *angle, double crX0, double crY0, double crRadius, boolean above,
	int tbLeftPosition, int tbRightPosition, int tbLowerPosition, int tbUpperPosition, bool paint) {

		int subsampling = SubsamplingEdit->Text.ToIntDef(10);
		int xOffset = subsampling/2+tbLeftPosition;
		int yOffset =  subsampling/2+tbUpperPosition;

		double ** x = largestOutline->createDataForFit((crX0-xOffset)/subsampling, (crY0-yOffset)/subsampling, crRadius/subsampling);

 //		largestOutline->drawOutline(bmpCanvas->line,x,largestOutline->getNumberOfPoints()); for testing purposes

		try {
			if(largestOutline->nPointsMinusCR > 5) {
				double xc,yc,rA,rB,ang;
				FitEllipse(x, largestOutline->nPointsMinusCR,&xc,&yc,&rA,&rB,&ang);
				xc = xc*subsampling + xOffset;
				yc = yc*subsampling + yOffset;
				rA = rA*subsampling;
				rB = rB*subsampling;
				ang = ang;
				//Begin transform to standard rotation

				int nPoints =4;
				double distanceSq = 0.0;
				double maxDistanceSq = 0.0;
				int maxI = -1;

				for(int i=0; i<nPoints; i++) {
					double tempX = rA*cos(2*PI*i/nPoints);
					double tempY = rB*sin(2*PI*i/nPoints);
					double x =  tempX*cos(ang)-tempY*sin(ang)+xc;
					double y =  tempX*sin(ang)+tempY*cos(ang)+yc;
				//	distanceSq = (x-380)*(x-380) + (y-350)*(y-350);
				   	distanceSq = (x-crX0)*(x-crX0) + (y-crY0)*(y-crY0);
					if(distanceSq > maxDistanceSq) {
						maxI = i;
						maxDistanceSq = distanceSq;
					}
				}
				//if maxI == 0 do nothing
				if(maxI == 1) {
					ang += PI/2.0;
					double temp = rA;
					rA = rB;
					rB = temp;
				}
				if(maxI == 2) {
					ang += PI;
				}
				if(maxI == 3) {
					ang += 3.0*PI/2.0;
					double temp = rA;
					rA = rB;
					rB = temp;
				}
				while(ang < 0.0) ang += 2*PI;
				while(ang >= 2*PI) ang -= 2*PI;
				//end transform to standard rotation


				*x0 = xc;
				*y0 = yc;
				*radiusA = rA;
				*radiusB = rB;
				*angle = ang;
				/*
				*x0 = xc*subsampling + xOffset;
				*y0 = yc*subsampling + yOffset;
				*radiusA = rA*subsampling;
				*radiusB = rB*subsampling;
				*angle = ang;
				*/


				if(above) {
					*radiusA *= crRadiusMultiplier;
					*radiusB *= crRadiusMultiplier;
					if(*radiusA > crRadiusMax) *radiusA = crRadiusMax;
					if(*radiusB > crRadiusMax) *radiusB = crRadiusMax;
				}
				if(paint) {
					if(above)
						drawEllipse(bmpCanvas, *x0/spatialDivisor, *y0/spatialDivisor, *radiusA/spatialDivisor, *radiusB/spatialDivisor, *angle, makecol(0,255,0));
					else
						drawEllipse(bmpCanvas, *x0/spatialDivisor, *y0/spatialDivisor, *radiusA/spatialDivisor, *radiusB/spatialDivisor, *angle, makecol(255,0,0));

				}
			}
		} catch (exception& ex) {
 //			printf("caught umaincapture\n");
 //			printf("exception: %s\n", ex.what());
			*x0 = *y0 = *radiusA = *radiusB = *angle = 0.0;
		}
		delete2D(x, 2);

		delete largestOutline;

 }

double xScene = 0.0;
double yScene = 0.0;
int goodFrames = 0;
double goodTime = 0.0;
double frameRate = 30;

double firstTimestamp, oldTimestamp, currentTimestamp;

double x0scene = 0.0;
double y0scene = 0.0;
double radiusAscene = 0.0;
double radiusBscene = 0.0;
double angleScene = 0.0;

void __fastcall TMainCaptureForm::DoFrame(BITMAP *aBmp)
{

	bool paint = false;
	if(nFrames % frameDivisor == 0)	paint = true;

	if(nFrames == 0) {
		firstTimestamp = lsl_local_clock();//ds_TimestampMs();
		oldTimestamp = firstTimestamp;
	 } else {
		currentTimestamp = lsl_local_clock();//ds_TimestampMs();
		double interval = (currentTimestamp - oldTimestamp);

		//if frame interval is reasonable, use it to estimate frame rate.
		if(RoundTo(interval*frameRate,0) == 1) {
			goodFrames++;
			goodTime += interval;
			frameRate = goodFrames/goodTime;
		}
		oldTimestamp = currentTimestamp;
		double dropped = (currentTimestamp - firstTimestamp)*frameRate  - nFrames;
		droppedFramesEdit->Text =  FormatFloat ("0", dropped);
	}

	if(paint) {
		stretch_blit(aBmp, bmpCanvas,
			0,0, aBmp->w, aBmp->h,
			0,0, bmpCanvas->w,bmpCanvas->h);
	}

	//to prevent the definition from begin changed during the calculation.
	int tbUpperPosition = tbUpper->Position;
	int tbLowerPosition = tbLower->Position;
	int tbLeftPosition = tbLeft->Position;
	int tbRightPosition = tbRight->Position;

	int crRoiLeftPosition = crRoiLeft->Position;
	int crRoiRightPosition = crRoiRight->Position;
	int crRoiTopPosition = crRoiTop->Position;
	int crRoiBottomPosition = crRoiBottom->Position;


	static int n=0;
	n++;


	if (isKind == isSceneCalib)  // Scene, Calibration mode
	{
		double x0, y0, radius;
//		findAndFitCircle(aBmp, &x0, &y0, &radius, 0.0,0.0,0.0, true,crRoiLeftPosition, crRoiRightPosition, crRoiBottomPosition, crRoiTopPosition, paint);

		TOutline* largestOutline =  findLargestOutline(aBmp, true,
			crRoiLeftPosition, crRoiRightPosition, crRoiBottomPosition, crRoiTopPosition, paint);

		fitCircle(aBmp, largestOutline, &x0, &y0, &radius, 0.0, 0.0, 0.0, true,
			crRoiLeftPosition, crRoiRightPosition, crRoiBottomPosition, crRoiTopPosition, paint);

		float sample [4];

		sample[0] = nFrames;
		sample[1] = x0;
		sample[2] = y0;
		sample[3] = radius;

		lsl_push_sample_ftp(outlet, sample, lsl_local_clock(), 1);

  /*		FrameStamp.drf.Event=(nFrames % 10 == 0) ? nFrames : 0;
		FrameStamp.drf.item_size = 4;// default is 4
		FrameStamp.drf.nItems = 10;//set to make visible in stream viewer;
		FrameStamp.Data[0] = (int) (x0*100000.0); //trick to save precision in int form
		FrameStamp.Data[1] = (int) (y0*100000.0);

		FrameStamp.Data[2] = (int) (radius*100000.0);
		FrameStamp.Data[3] = 0;
		FrameStamp.Data[4] = 0;
		FrameStamp.Data[5] = 0;

		FrameStamp.Data[6] =0;
		FrameStamp.Data[7] =0;
		FrameStamp.Data[8] =0;
		FrameStamp.Data[9] =0;

		FrameStamp.drf.TimeStampOrig= ds_TimestampMs();
		FrameStamp.TimeStampRecv= FrameStamp.drf.TimeStampOrig;
		ds_Write(handleWrScene,(char *)&FrameStamp,	sizeof FrameStamp);
	*/
		nFrames++;
		if(paint) {
			hline(bmpCanvas, crRoiLeftPosition/spatialDivisor, crRoiTopPosition/spatialDivisor, crRoiRightPosition/spatialDivisor, makecol(255,0,0));
			hline(bmpCanvas, crRoiLeftPosition/spatialDivisor, crRoiBottomPosition/spatialDivisor, crRoiRightPosition/spatialDivisor, makecol(255,0,0));
			vline(bmpCanvas, crRoiLeftPosition/spatialDivisor,crRoiTopPosition/spatialDivisor, crRoiBottomPosition/spatialDivisor, makecol(255,0,0));
			vline(bmpCanvas, crRoiRightPosition/spatialDivisor,crRoiTopPosition/spatialDivisor, crRoiBottomPosition/spatialDivisor, makecol(255,0,0));

			hline(bmpCanvas, 0, y0/spatialDivisor, bmpCanvas->w, makecol(0,255,255));
			vline(bmpCanvas, x0/spatialDivisor, 0, bmpCanvas->h, makecol(0,255,255));

			HWND hWnd = tPanel->Handle;
			HDC hDC = GetDC(hWnd);
			draw_to_hdc (hDC,bmpCanvas,0,0);
			ReleaseDC(hWnd,hDC);
		}
		return;
	}
	else if (isKind == isScene) {
		float data[6];
		int errcode;
		while(lsl_pull_sample_f(gazestreamThread->inlet,data, 6, 0.0, &errcode));
		x0scene = data[1];
		y0scene = data[2];
		radiusAscene = data[3];
		radiusBscene = data[4];
		angleScene = data[5];
   /*		TMaxArray fr;

		while (ds_Read(handleRd,(char *)&fr)) {
			x0scene = fr.Data[0]/100000.0;
			y0scene = fr.Data[1]/100000.0;
			radiusscene = fr.Data[2]/100000.0;
		}
	 */

		double xMonitor = x0scene;
		double yMonitor = y0scene;
		double xScene = 0.0;
		double yScene = 0.0;

		if(gu->rEye != 0.0 && radiusAscene !=0 && radiusBscene != 0) {
				gu->inverseEyeMap(&xMonitor,&yMonitor);
				xScene = xMonitor;
				yScene = yMonitor;
				gu->sceneMap(&xScene, &yScene);

		}

		xScene += xParallaxCorrection;
		yScene += yParallaxCorrection;

		CaptureWorkerForm->xScene = xScene;
		CaptureWorkerForm->yScene = yScene;

		float sample [12];


		sample[0] = nFrames;
		sample[1] = x0scene; //pixels
		sample[2] = y0scene; //pixels
		sample[3] = (1.0 - xMonitor/gu->monitorWidth); //percentage
		sample[4] = (1.0 - yMonitor/gu->monitorHeight);//percentage
		sample[5] = radiusAscene; //pixels
		sample[6] = radiusBscene; //pixels
		sample[7] = angleScene; //radius

		sample[8] = xScene; //pixels
		sample[9] = yScene; //pixels

		sample[10] = xParallaxCorrection; //pixels
		sample[11] = yParallaxCorrection; //pixels


		lsl_push_sample_ftp(outlet, sample, lsl_local_clock(), 1);

  /*		FrameStamp.drf.Event=(nFrames % 10 == 0) ? nFrames : 0;
		FrameStamp.drf.item_size = 4;// default is 4
		FrameStamp.drf.nItems = 10;//set to make visible in stream viewer;

		FrameStamp.Data[0] = (int) (x0scene*100000.0); //trick to save precision in int form, mm*100000
		FrameStamp.Data[1] = (int) (y0scene*100000.0); //mm*10000
		FrameStamp.Data[2] =(int) (100000.0*radiusscene); //pixels

		FrameStamp.Data[3] = (int) (1000.0 - 1000.0*xMonitor/gu->monitorWidth); //to pixels
		FrameStamp.Data[4] = (int) (1000.0 - 1000.0*yMonitor/gu->monitorHeight);//to pixels

		FrameStamp.Data[5] = (int) (xScene*100000.0); //mm*10000
		FrameStamp.Data[6] = (int) (yScene*100000.0); //mm*10000

		FrameStamp.Data[7] =(int) (xParallaxCorrection*100000.0);
		FrameStamp.Data[8] =(int) (yParallaxCorrection*100000.0);
		FrameStamp.Data[9] =0;

		FrameStamp.drf.TimeStampOrig= ds_TimestampMs();
		FrameStamp.TimeStampRecv= FrameStamp.drf.TimeStampOrig;
		ds_Write(handleWrScene,(char *)&FrameStamp,	sizeof FrameStamp);
  */
		nFrames++;
		if(paint) {
			circlefill(bmpCanvas, xScene/spatialDivisor, yScene/spatialDivisor, 5, makecol(0,0,0));
			circlefill(bmpCanvas, xScene/spatialDivisor, yScene/spatialDivisor, 3, makecol(255,255,0));
			HWND hWnd = tPanel->Handle;
			HDC hDC = GetDC(hWnd);
			draw_to_hdc (hDC,bmpCanvas,0,0);
			ReleaseDC(hWnd,hDC);
		}
	}

	else if (isKind == isVideo)  // Video
	{
  //	printf("%g\n", lsl_local_clock());

		lsl_push_sample_itp(outlet, &(nFrames), lsl_local_clock(),1);
		nFrames++;
 /*		FrameNumber.drf.Event = nFrames;
		FrameNumber.drf.TimeStampOrig= ds_TimestampMs();
		FrameNumber.TimeStampRecv= FrameNumber.drf.TimeStampOrig;

		edTimestamp->Text = FrameNumber.drf.TimeStampOrig;
		edFrame->Text = nFrames;

		Zero.drf.Event = 0;
		Zero.drf.TimeStampOrig= ds_TimestampMs();
		Zero.TimeStampRecv= FrameNumber.drf.TimeStampOrig;

		int sz = Zero.GetUsedSizeInBytes();

		if (nFrames  % 10 == 0)
			ds_Write(handleWrVideo,(char *)&FrameNumber,sz);
		else
			ds_Write(handleWrVideo,(char *)&Zero,sz);
	   */
		if(paint) {

			HWND hWnd = tPanel->Handle;
			HDC  hDC  = GetDC(hWnd);
			draw_to_hdc (hDC,bmpCanvas,0,0);
			ReleaseDC(hWnd,hDC);
		}
		return;
	} else if (isKind == isEye) {

	   //	TMaxArray fr;

		bool pr = (nFrames%30 == 0);
		double x0 = 0.0, y0 = 0.0, radius = 0.0;
		double radiusA = 0.0, radiusB = 0.0, angle = 0.0;
		double crX0 = 0.0, crY0 = 0.0, crRadius = 0.0;
		//find cornea reflection
		TOutline* largestOutline =  findLargestOutline(aBmp, true,
			crRoiLeftPosition, crRoiRightPosition, crRoiBottomPosition, crRoiTopPosition, paint);

		fitCircle(aBmp, largestOutline, &crX0, &crY0, &crRadius, 0.0, 0.0, 0.0, true,
			crRoiLeftPosition, crRoiRightPosition, crRoiBottomPosition, crRoiTopPosition, paint);

		//find pupil, don't fit near cornea reflection

		largestOutline =  findLargestOutline(aBmp, false,
			tbLeftPosition, tbRightPosition, tbLowerPosition,tbUpperPosition, paint);

		fitEllipse(aBmp, largestOutline, &x0, &y0, &radiusA, &radiusB, &angle, crX0, crY0, crRadius, false,
			tbLeftPosition, tbRightPosition, tbLowerPosition, tbUpperPosition, paint);

		if(storeFit) {
			ds_Ellipse ellipse;
			ellipse.x0 = x0;
			ellipse.y0 = y0;
			ellipse.a = radiusA;
			ellipse.b = radiusB;
			ellipse.angle = angle;
			storedPupils.push_back(ellipse);
			storeFit = false;
		}


		double xMonitor = x0;
		double yMonitor = y0;
		double xScene = 0.0;
		double yScene = 0.0;
		double xRotatedMonitor = 0.0;
		double yRotatedMonitor = 0.0;

		if(gu->rEye != 0.0 && radius !=0 ) {
				gu->inverseEyeMap(&xMonitor,&yMonitor);
				xScene = xMonitor;
				yScene = yMonitor;
				gu->sceneMap(&xScene, &yScene);

		}

		if(pr && CONSOLE) printf("x0: %g\n", x0);
		if(pr && CONSOLE) printf("y0: %g\n", y0);

		float sample [6];

		sample[0] = nFrames;
		sample[1] = x0; //mm
		sample[2] = y0; //mm
		sample[3] = radiusA;//pixels
		sample[4] = radiusB;//pixels
		sample[5] = angle;//radians



		lsl_push_sample_ftp(outlet, sample, lsl_local_clock(), 1);
/*
		FrameStamp.drf.Event = (nFrames % 10 == 0) ? nFrames : 0;
		FrameStamp.drf.item_size = 4;// default is 4
		FrameStamp.drf.nItems = 10;//set to make visible in stream viewer;
		FrameStamp.Data[0] = (int) (x0*100000.0); //trick to save precision in int form, mm*100000
		FrameStamp.Data[1] = (int) (y0*100000.0); //mm*10000

		FrameStamp.Data[2] = (int) (100000.0*radiusA); //pixels
		FrameStamp.Data[3] = (int) (100000.0*radiusB); //pixels

		FrameStamp.Data[4] = (int) (100000.0*angle); //radians
		FrameStamp.Data[5] = 0;

		FrameStamp.Data[6] =0;
		FrameStamp.Data[7] =0;
		FrameStamp.Data[8] =0;
		FrameStamp.Data[9] =0;

		FrameStamp.drf.TimeStampOrig= ds_TimestampMs();
		FrameStamp.TimeStampRecv= FrameStamp.drf.TimeStampOrig;
		ds_Write(handleWr1,(char *)&FrameStamp,	sizeof FrameStamp);
  */

		nFrames++;
		if(paint) {

			/////drawing ellipse lines
		  for(std::list<ds_Ellipse>::iterator ellp = storedPupils.begin(); ellp != storedPupils.end(); ++ellp){
				ds_Ellipse ell = *ellp;
				line(bmpCanvas, ell.x0-20*ell.a*cos(ell.angle), ell.y0-20*ell.a*sin(ell.angle), ell.a*cos(ell.angle)+ell.x0, ell.a*sin(ell.angle)+ell.y0, makecol(0,0,0));
			}
			/////end drawing ellipse lines



			hline(bmpCanvas, tbLeft->Position/spatialDivisor, tbUpper->Position/spatialDivisor, tbRight->Position/spatialDivisor, makecol(0,255,0));
			hline(bmpCanvas, tbLeft->Position/spatialDivisor, tbLower->Position/spatialDivisor, tbRight->Position/spatialDivisor, makecol(0,255,0));
			vline(bmpCanvas, tbLeft->Position/spatialDivisor,tbUpper->Position/spatialDivisor, tbLower->Position/spatialDivisor, makecol(0,255,0));
			vline(bmpCanvas, tbRight->Position/spatialDivisor,tbUpper->Position/spatialDivisor, tbLower->Position/spatialDivisor, makecol(0,255,0));

			hline(bmpCanvas, crRoiLeft->Position/spatialDivisor, crRoiTop->Position/spatialDivisor, crRoiRight->Position/spatialDivisor, makecol(255,0,0));
			hline(bmpCanvas, crRoiLeft->Position/spatialDivisor, crRoiBottom->Position/spatialDivisor, crRoiRight->Position/spatialDivisor, makecol(255,0,0));
			vline(bmpCanvas, crRoiLeft->Position/spatialDivisor,crRoiTop->Position/spatialDivisor, crRoiBottom->Position/spatialDivisor, makecol(255,0,0));
			vline(bmpCanvas, crRoiRight->Position/spatialDivisor,crRoiTop->Position/spatialDivisor, crRoiBottom->Position/spatialDivisor, makecol(255,0,0));

			hline(bmpCanvas, 0, y0/spatialDivisor, bmpCanvas->w, makecol(0,255,255));
			vline(bmpCanvas, x0/spatialDivisor, 0, bmpCanvas->h, makecol(0,255,255));

			circlefill(bmpCanvas, x0/spatialDivisor, y0/spatialDivisor, 3, makecol(255,255,0));

			HWND hWnd = tPanel->Handle;
			HDC hDC = GetDC(hWnd);
			draw_to_hdc (hDC,bmpCanvas,0,0);
			ReleaseDC(hWnd,hDC);
		}
		return;
	}

}

void __fastcall TMainCaptureForm::cbRecordClick(TObject *Sender)
{
	nFrames=0;
	edFrame->Text = nFrames;
	BitBtnPlay->Glyph = cbRecord->Checked ? pBmpRec:pBmpRecGr;
}


void __fastcall TMainCaptureForm::BitBtnPlayClick(TObject *Sender)
{

  if(isKind == isVideo) {
		if(outlet) {
			lsl_destroy_outlet(outlet);
			outlet = NULL;
		}


		lsl_streaminfo info = lsl_create_streaminfo("VideoStream","VideoStream",1,30,cft_int32,"");
		lsl_xml_ptr desc = lsl_get_desc(info);
		lsl_xml_ptr chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","frame");
		lsl_append_child_value(chn,"unit","number");

		outlet = lsl_create_outlet(info,0,360);
	 /*	if(handleWrVideo) ds_Close(handleWrVideo);
		char tmpString[256];
		sprintf(tmpString, "/tmp/VideoStream%d",CaptureWorkerForm->VideoGrabber->VideoDevice);
		handleWrVideo = ds_Open(tmpString);
		ds_XMLSetSamplerate(handleWrVideo,30);
		ds_XMLSetDatasize(handleWrVideo, 0, 4);
		sprintf(tmpString, "/tmp/VideoStream%d.xml",CaptureWorkerForm->VideoGrabber->VideoDevice);
		WriteXMLtoFile(handleWrVideo,tmpString);
		*/
	}
	cbVideoInputDevice->Enabled = false;
	cbVideoInputFormat->Enabled = false;
	SpatialDivisorEdit->Enabled = false;

	Start();
	frameThread = new TFrameThread(this, bmpQueue, hMutex, false);
	BitBtnStop->Enabled=true;
	BitBtnPlay->Enabled=false;

	edtFrameRate->Text = FormatFloat ("0.00", CaptureWorkerForm->VideoGrabber->CurrentFrameRate);

}
//---------------------------------------------------------------------------



double alphaX = 0;
double alphaY = 0;
int posX;
int posY;

void __fastcall TMainCaptureForm::Timer1Timer(TObject *Sender)
{
	if(frameThread) {
		BacklogEdit->Text = bmpQueue.size();
	}
}
//---------------------------------------------------------------------------









void __fastcall TMainCaptureForm::RadioGroup1Click(TObject *Sender)
{
	//eye camera
	if (RadioGroup1->ItemIndex==0)
	{
		isKind = isEye;
		PageControl2->ActivePage = tsEyeTracker;
		Caption = "Video stream: eye camera";

		/*
		TXMLDocument * xml =CreateXML(MainCaptureForm);

		handleWr1 = ds_Open("/tmp/GazeStream");
		ds_XMLSetDatasize(handleWr1, 10, 4);
		ds_XMLSetSamplerate(handleWr1,30);
		WriteXMLtoFile(handleWr1,"/tmp/GazeStream.xml");
		*/

		if(outlet) {
			lsl_destroy_outlet(outlet);
			outlet = NULL;
		}
		lsl_streaminfo info = lsl_create_streaminfo("GazeStream","GazeStream",6,30,cft_float32,generateGUID());

		lsl_xml_ptr desc = lsl_get_desc(info);
		lsl_xml_ptr chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","frame");
		lsl_append_child_value(chn,"unit","number");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","pupil position x");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","pupil position y");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","pupil radius A");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","pupil radius B");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","pupil angle");
		lsl_append_child_value(chn,"unit","radians");
		outlet = lsl_create_outlet(info,0,360);


	}
	else
	//scene camera calibrate
	if (RadioGroup1->ItemIndex==1)
	{



		crRoiLeft->Position = 40;
		crRoiRight->Position = 600;
		crRoiTop->Position = 40;
		crRoiBottom->Position = 440;
		isKind = isSceneCalib;
		PageControl2->ActivePage = tsEyeTracker;
		Caption = "Video stream: scene camera";


		if(outlet) {
			lsl_destroy_outlet(outlet);
			outlet = NULL;
		}
		lsl_streaminfo info = lsl_create_streaminfo("SceneCalibrateStream","SceneCalibrateStream",4,30,cft_float32,generateGUID());
		lsl_xml_ptr desc = lsl_get_desc(info);
		lsl_xml_ptr chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","frame");
		lsl_append_child_value(chn,"unit","number");

		lsl_append_child_value(chn, "name","scene position x");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","scene position y");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","radius");
		lsl_append_child_value(chn,"unit","pixels");

		outlet = lsl_create_outlet(info,0,360);

	/*	handleWrScene = ds_Open("/tmp/SceneCalibrateStream");
		ds_XMLSetSamplerate(handleWrScene,30);
		ds_XMLSetDatasize(handleWrScene, 10, 4);
		WriteXMLtoFile(handleWrScene,"/tmp/SceneCalibrateStream.xml");
	  */
	}
	else
	//scene display
	if (RadioGroup1->ItemIndex==2)
	{

		isKind = isScene;
		PageControl2->ActivePage = tsScene;
		Caption = "Video stream: scene camera";

		if(outlet) {
			lsl_destroy_outlet(outlet);
			outlet = NULL;
		}
		lsl_streaminfo info = lsl_create_streaminfo("SceneStream","SceneStream",12,30,cft_float32,generateGUID());
		lsl_xml_ptr desc = lsl_get_desc(info);
		lsl_xml_ptr chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","frame");
		lsl_append_child_value(chn,"unit","number");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","pupil position x");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","pupil position y");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","screen position x");
		lsl_append_child_value(chn,"unit","x/monitorWidth");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","screen position y");
		lsl_append_child_value(chn,"unit","y/monitorHeight");


		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","pupil radius A");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","pupil radius B");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","pupil angle");
		lsl_append_child_value(chn,"unit","radians");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","scene position x");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","scene position y");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","parallax correction x");
		lsl_append_child_value(chn,"unit","pixels");

		chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","parallax correction y");
		lsl_append_child_value(chn,"unit","pixels");

		outlet = lsl_create_outlet(info,0,360);

		gazestreamThread = new TStreamThread("type='GazeStream'");


	/*
		handleRd = ds_Open("/tmp/GazeStream");

		handleWrScene = ds_Open("/tmp/SceneStream");
		ds_XMLSetSamplerate(handleWrScene,30);
		ds_XMLSetDatasize(handleWrScene, 10, 4);
		WriteXMLtoFile(handleWrScene,"/tmp/SceneStream.xml");

		*/
	}
	RadioGroup1->Enabled=false;


}


void TMainCaptureForm::SetToVideoMode() {
		isKind = isVideo;
		PageControl2->ActivePage = tsVideo;
		Caption = "Video stream: video camera";

  /*		if(outlet) {
			lsl_destroy_outlet(outlet);
			outlet = NULL;
		}
		lsl_streaminfo info = lsl_create_streaminfo("VideoStream","VideoStream",1,30,cft_int32,"");

		lsl_xml_ptr desc = lsl_get_desc(info);
		lsl_xml_ptr chn = lsl_append_child(desc, "channel");
		lsl_append_child_value(chn, "name","frame");
		lsl_append_child_value(chn,"unit","number");
		outlet = lsl_create_outlet(info,0,360);
	   */
		RadioGroup1->Visible = false;

		allegro_init();
		set_gdi_color_format();
		set_color_depth(CDEPTH);
		bmpCanvas = create_bitmap_ex(CDEPTH,tPanel->Width,tPanel->Height);
		clear_bitmap(bmpCanvas);
}
//---------------------------------------------------------------------------

void __fastcall TMainCaptureForm::FormClose(TObject *Sender, TCloseAction &Action)
{

	if(BitBtnStop->Enabled) btStopClick(this);
	if(frameThread) {
		frameThread->Terminate();
		delete frameThread; //will not delete till terminated, by VCL design.
		frameThread = NULL;
	}

	delete pBmpRec;
	delete pBmpRecGr;
	delete pBmpPau;
	delete pBmpPauGr;
	if(bmpCanvas) {
		destroy_bitmap(bmpCanvas);
		bmpCanvas = NULL;
	}

}
//---------------------------------------------------------------------------





void __fastcall TMainCaptureForm::LoadCalibrationClick(TObject *Sender)
{
	if(OpenDialog1->Execute()) {
		gu->LoadGazeCalibration(OpenDialog1->FileName, xdoc_in);

	}
}
//---------------------------------------------------------------------------




void __fastcall TMainCaptureForm::crRadiusMultiplierEditChange(TObject *Sender)
{
	bool ex = false;
	try {
		crRadiusMultiplierEdit->Text.ToDouble();
	} catch (...) {
		ex = true;
	}

	if(!ex) crRadiusMultiplier = crRadiusMultiplierEdit->Text.ToDouble();
}


//---------------------------------------------------------------------------

void __fastcall TMainCaptureForm::nOutlinesEditChange(TObject *Sender)
{
	bool ex = false;
	try {
		nOutlinesEdit->Text.ToInt();
	} catch (...) {
		ex = true;
	}

	if(!ex) nOutlinesDesired = nOutlinesEdit->Text.ToInt();
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------

void __fastcall TMainCaptureForm::crRadiusMaxEditChange(TObject *Sender)
{
	bool ex = false;
	try {
		crRadiusMaxEdit->Text.ToDouble();
	} catch (...) {
		ex = true;
	}

	if(!ex) crRadiusMax = crRadiusMaxEdit->Text.ToDouble();
}
//---------------------------------------------------------------------------

void __fastcall TMainCaptureForm::FrameDivisorEditChange(TObject *Sender)
{
	bool ex = false;
	int temp;
	try {
		temp = FrameDivisorEdit->Text.ToInt();

	} catch (...) {
		ex = true;
	}
	if(!ex && temp > 0) frameDivisor = temp;
}
//---------------------------------------------------------------------------

void __fastcall TMainCaptureForm::SpatialDivisorEditChange(TObject *Sender)
{
	bool ex = false;
	int temp;
	try {
		temp = SpatialDivisorEdit->Text.ToInt();

	} catch (...) {
		ex = true;
	}
	if(!ex && temp > 0) {
		spatialDivisor = temp;
		cbVideoInputFormatChange(this);
	}
}
//---------------------------------------------------------------------------

void __fastcall TMainCaptureForm::maxEccentricityEditChange(TObject *Sender)
{
	bool ex = false;
	try {
		maxEccentricityEdit->Text.ToDouble();
	} catch (...) {
		ex = true;
	}

	if(!ex) maxEccentricity = maxEccentricityEdit->Text.ToDouble();
}
//---------------------------------------------------------------------------





void __fastcall TMainCaptureForm::xParallaxCorrectionEditChange(TObject *Sender)
{
	bool ex = false;
	try {
		xParallaxCorrectionEdit->Text.ToDouble();
	} catch (...) {
		ex = true;
	}

	if(!ex) xParallaxCorrection = xParallaxCorrectionEdit->Text.ToDouble();
}
//---------------------------------------------------------------------------

void __fastcall TMainCaptureForm::yParallaxCorrectionEditChange(TObject *Sender)
{
	bool ex = false;
	try {
		yParallaxCorrectionEdit->Text.ToDouble();
	} catch (...) {
		ex = true;
	}

	if(!ex) yParallaxCorrection = -yParallaxCorrectionEdit->Text.ToDouble();
}
//---------------------------------------------------------------------------



