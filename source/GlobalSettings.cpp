
#include "GlobalSettings.h"

double GlobalSettings::startTime = 0.0;
double GlobalSettings::endTime = 0.0;
double GlobalSettings::framesFront = 0;
double GlobalSettings::framesBack = 0;
MColor GlobalSettings::pathColor = MColor(0.5f, 0.5f, 0.8f);
MColor GlobalSettings::currentFrameColor =  MColor(0.8f, 0.8f, 0.1f);
MColor GlobalSettings::tangentColor =  MColor(0.5f, 0.7f, 0.1f);
MColor GlobalSettings::brokenTangentColor =  MColor(0.1f, 0.5f, 0.7f);
MColor GlobalSettings::bufferPathColor = MColor(0.2f, 0.2f, 0.2f);
MColor GlobalSettings::weightedPathTangentColor = MColor(0.2f, 0.2f, 0.2f);
MColor GlobalSettings::weightedPathColor = MColor(0.2f, 0.2f, 0.2f);
MColor GlobalSettings::frameLabelColor = MColor(0.1f, 0.1f, 0.1f);
MColor GlobalSettings::keyframeLabelColor = MColor(1.0f, 1.0f, 0.0f);  // Yellow for keyframe numbers
double GlobalSettings::pathSize = 3.0;
double GlobalSettings::frameSize = 7.0;
double GlobalSettings::keyframeLabelSize = 1.2;  // Slightly larger for keyframes
double GlobalSettings::frameLabelSize = 0.8;     // Normal size for regular frames
bool GlobalSettings::showTangents = true;
bool GlobalSettings::showKeyFrames = true;
bool GlobalSettings::showKeyFrameNumbers = false;
bool GlobalSettings::showFrameNumbers = false;
bool GlobalSettings::showRotationKeyFrames = true;
bool GlobalSettings::showPath = true;
double GlobalSettings::drawTimeInterval = 0.1;
int GlobalSettings::drawFrameInterval = 1;  // Default 1 frame interval
MMatrix GlobalSettings::cameraMatrix;
int GlobalSettings::portWidth = 0;
int GlobalSettings::portHeight = 0;
bool GlobalSettings::lockedMode = false;
bool GlobalSettings::enabled = false;
bool GlobalSettings::alternatingFrames = false;
bool GlobalSettings::lockedModeInteractive = true;
bool GlobalSettings::usePivots = false;
int GlobalSettings::strokeMode = 0;
GlobalSettings::DrawMode GlobalSettings::motionPathDrawMode = GlobalSettings::kWorldSpace;

// Draw preview settings
int GlobalSettings::drawKeyframeCount = 5;
MColor GlobalSettings::previewPathColor = MColor(1.0f, 0.5f, 0.2f, 0.6f);  // Orange, semi-transparent
MColor GlobalSettings::previewKeyframeColor = MColor(1.0f, 0.8f, 0.2f);   // Yellow-orange
